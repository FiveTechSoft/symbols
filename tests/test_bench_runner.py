#!/usr/bin/env python3
"""Fixtures and mutants for tools/bench_runner.py (M3 C5).

A synthetic fixture benchmark emits a known sample set; the runner must
reproduce exact statistics. Each mutant (nonzero exit, missing BENCH_DONE,
malformed or undeclared SAMPLE lines, too few samples, missing environment)
must be refused with exit 2 and no record file. The memory high-water is
asserted against a child that allocates a known block. Exits 0 on success,
1 on failure.
"""
import json
import math
import os
import platform
import statistics
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import bench_runner  # noqa: E402

RUNNER = os.path.join(ROOT, 'tools', 'bench_runner.py')

FIXTURE = r'''
import sys
mode = sys.argv[1] if len(sys.argv) > 1 else "ok"
if mode == "mem":
    buf = bytearray(8 * 1024 * 1024)
    for i in range(0, len(buf), 4096):
        buf[i] = 1
    print("allocated", len(buf))
    for i in range(40):
        print("SAMPLE work %d" % (1000 + i * 10))
    print("BENCH_DONE")
elif mode == "exit1":
    for i in range(40):
        print("SAMPLE work %d" % (1000 + i * 10))
    print("BENCH_DONE")
    sys.exit(1)
elif mode == "nodone":
    for i in range(40):
        print("SAMPLE work %d" % (1000 + i * 10))
    sys.exit(0)
elif mode == "malformed":
    for i in range(39):
        print("SAMPLE work %d" % (1000 + i * 10))
    print("SAMPLE work notanumber")
    print("BENCH_DONE")
    sys.exit(0)
elif mode == "undeclared":
    for i in range(40):
        print("SAMPLE work %d" % (1000 + i * 10))
    print("SAMPLE other 100")
    print("BENCH_DONE")
    sys.exit(0)
elif mode == "few":
    for i in range(10):
        print("SAMPLE work %d" % (1000 + i * 10))
    print("BENCH_DONE")
    sys.exit(0)
elif mode == "crlf":
    sys.stdout.flush()
    import os as _os
    for i in range(40):
        _os.write(1, ("SAMPLE work %d\r\n" % (1000 + i * 10)).encode())
    _os.write(1, b"BENCH_DONE\r\n")
    sys.exit(0)
else:
    print("human chatter line, ignored")
    for i in range(40):
        print("SAMPLE work %d" % (1000 + i * 10))
    print("BENCH_DONE")
'''

CACHE = ('CMAKE_C_COMPILER_ID:STRING=GNU\n'
         'CMAKE_C_COMPILER_VERSION:STRING=13.3.0\n'
         'CMAKE_BUILD_TYPE:STRING=Release\n')

FAILURES = []


def check(name, cond, detail=''):
    if cond:
        print('ok: %s' % name)
    else:
        print('FAIL: %s %s' % (name, detail))
        FAILURES.append(name)


def write_fixture(tmp):
    path = os.path.join(tmp, 'fixture_bench.py')
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(FIXTURE)
    build = os.path.join(tmp, 'build')
    os.makedirs(build)
    with open(os.path.join(build, 'CMakeCache.txt'), 'w', encoding='utf-8') as f:
        f.write(CACHE)
    return path, build


def run(tmp, fixture, build, mode, out_name='record.json', extra=None,
        subject='a' * 40):
    out = os.path.join(tmp, out_name)
    argv = [sys.executable, RUNNER, '--exe', sys.executable,
            '--exe-arg', fixture, '--exe-arg', mode,
            '--build-dir', build, '--fixture-id', 'synthetic',
            '--corpus-version', 'seed:42', '--min-samples', '30',
            '--phase', 'work', '--out', out, '--subject-sha', subject]
    if extra:
        argv += extra
    p = subprocess.run(argv, capture_output=True, text=True, timeout=120)
    return p, out


def expect_refuse(tmp, name, mode, extra=None):
    p, out = run(tmp, *FIX_ARGS, mode, out_name='refuse_%s.json' % name, extra=extra)
    check('refuse %s' % name, p.returncode == 2 and not os.path.exists(out),
          'rc=%d stderr=%s' % (p.returncode, p.stderr.strip()[:120]))


def main():
    global FIX_ARGS
    with tempfile.TemporaryDirectory() as tmp:
        fixture, build = write_fixture(tmp)
        FIX_ARGS = (fixture, build)

        # Happy path: exact statistics on a known sample set.
        p, out = run(tmp, fixture, build, 'ok')
        check('happy rc', p.returncode == 0, p.stderr.strip()[:200])
        if p.returncode == 0:
            rec = json.load(open(out, encoding='utf-8'))
            try:
                bench_runner.validate_record(rec)
                check('record validates', True)
            except bench_runner.Refuse as e:
                check('record validates', False, str(e))
            st = rec['phases']['work']
            samples = [1000 + i * 10 for i in range(40)]
            check('stats n', st['n'] == 40, str(st['n']))
            check('stats min/max', st['min'] == 1000 and st['max'] == 1390)
            check('stats median', st['median'] == statistics.median(samples),
                  str(st['median']))
            check('stats p95', st['p95'] == 1370, str(st['p95']))
            check('stats p99', st['p99'] == 1390, str(st['p99']))
            check('stats mean', abs(st['mean'] - statistics.fmean(samples)) < 1e-9)
            check('stats stdev', abs(st['stdev'] - statistics.pstdev(samples)) < 1e-9)
            env = rec['environment']
            check('env toolchain', env['toolchain'] == 'GNU 13.3.0', env['toolchain'])
            check('env build_type', env['build_type'] == 'Release')
            check('env os', env['os'] == platform.system(), env['os'])
            check('env cpu nonempty', len(env['cpu']) > 0)
            check('complete', rec['complete'] is True)
            check('memory method', rec['memory']['method'] in bench_runner.MEM_METHODS)
            check('memory positive', rec['memory']['peak_rss_bytes'] > 0,
                  str(rec['memory']['peak_rss_bytes']))
            check('unique keys roundtrip', len(rec) == 13)

        # CRLF line endings from the child still parse.
        p, out = run(tmp, fixture, build, 'crlf', out_name='crlf.json')
        check('crlf accepted', p.returncode == 0, p.stderr.strip()[:120])
        if p.returncode == 0:
            rec = json.load(open(out, encoding='utf-8'))
            check('crlf stats n', rec['phases']['work']['n'] == 40)

        # Memory high-water: a child allocating a known 8 MiB block.
        p, out = run(tmp, fixture, build, 'mem', out_name='mem.json')
        check('mem rc', p.returncode == 0, p.stderr.strip()[:120])
        if p.returncode == 0:
            rec = json.load(open(out, encoding='utf-8'))
            peak = rec['memory']['peak_rss_bytes']
            check('mem high-water >= 8 MiB', peak >= 8 * 1024 * 1024,
                  '%d bytes via %s' % (peak, rec['memory']['method']))

        # Mutants: every contract breach refuses with exit 2 and no record.
        expect_refuse(tmp, 'exit1', 'exit1')
        expect_refuse(tmp, 'nodone', 'nodone')
        expect_refuse(tmp, 'malformed', 'malformed')
        expect_refuse(tmp, 'undeclared', 'undeclared')
        expect_refuse(tmp, 'few', 'few')

        # Missing environment: no CMakeCache and no overrides must refuse.
        empty_build = os.path.join(tmp, 'emptybuild')
        os.makedirs(empty_build)
        out = os.path.join(tmp, 'noenv.json')
        p = subprocess.run([sys.executable, RUNNER, '--exe', sys.executable,
                            '--exe-arg', fixture, '--exe-arg', 'ok',
                            '--build-dir', empty_build, '--fixture-id', 'synthetic',
                            '--corpus-version', 'seed:42', '--min-samples', '30',
                            '--phase', 'work', '--out', out,
                            '--subject-sha', 'b' * 40],
                           capture_output=True, text=True, timeout=120)
        check('refuse no environment', p.returncode == 2 and not os.path.exists(out),
              'rc=%d' % p.returncode)

        # validate_record rejects tampered records.
        good = json.load(open(os.path.join(tmp, 'record.json'), encoding='utf-8'))
        tampered = dict(good)
        tampered['version'] = True
        try:
            bench_runner.validate_record(tampered)
            check('validate rejects bool version', False)
        except bench_runner.Refuse:
            check('validate rejects bool version', True)
        tampered = dict(good)
        tampered['extra'] = 1
        try:
            bench_runner.validate_record(tampered)
            check('validate rejects unknown key', False)
        except bench_runner.Refuse:
            check('validate rejects unknown key', True)
        tampered = json.loads(json.dumps(good))
        tampered['phases']['work']['n'] = 3
        try:
            bench_runner.validate_record(tampered)
            check('validate rejects n < min_samples', False)
        except bench_runner.Refuse:
            check('validate rejects n < min_samples', True)
        tampered = json.loads(json.dumps(good))
        tampered['phases']['work']['p99'] = 5
        try:
            bench_runner.validate_record(tampered)
            check('validate rejects ordering', False)
        except bench_runner.Refuse:
            check('validate rejects ordering', True)

    if FAILURES:
        print('FAILED: %d checks' % len(FAILURES))
        return 1
    print('all checks passed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
