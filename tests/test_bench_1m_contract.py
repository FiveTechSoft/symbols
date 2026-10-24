#!/usr/bin/env python3
"""Integration test: bench_1m under tools/bench_runner.py (M3 C5).

Runs the real benchmark at small sizes through the runner and requires a
valid claim-grade record: both phases present with enough samples, ordered
statistics, positive memory high-water, complete:true. Usage:
    test_bench_1m_contract.py <build_dir>
Exits 0 on success, 1 on failure.
"""
import json
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RUNNER = os.path.join(ROOT, 'tools', 'bench_runner.py')

FAILURES = []


def check(name, cond, detail=''):
    if cond:
        print('ok: %s' % name)
    else:
        print('FAIL: %s %s' % (name, detail))
        FAILURES.append(name)


def main():
    if len(sys.argv) != 2:
        print('usage: test_bench_1m_contract.py <build_dir>')
        return 1
    build = os.path.abspath(sys.argv[1])
    suffix = '.exe' if os.name == 'nt' else ''
    # Single-config generators place the exe at the build root; multi-config
    # generators (Visual Studio) place it under <config>/. Accept exactly one
    # candidate anywhere one level deep; ambiguous or missing fails closed.
    import glob as globmod
    pattern = os.path.join(build, '*', 'bench_1m' + suffix)
    found = [c for c in [os.path.join(build, 'bench_1m' + suffix)]
             if os.path.isfile(c)]
    found += [c for c in sorted(globmod.glob(pattern)) if os.path.isfile(c)]
    if len(found) != 1:
        print('FAIL: bench_1m candidates found: %s' % (found or 'none'))
        return 1
    exe = found[0]
    print('using benchmark: %s' % exe)
    # Multi-config trees carry the configuration in the exe's parent dir; the
    # runner must record it because the cache has no CMAKE_BUILD_TYPE there.
    config = os.path.basename(os.path.dirname(exe))
    extra = []
    if config in ('Release', 'Debug', 'RelWithDebInfo', 'MinSizeRel'):
        extra = ['--build-type', config]

    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, 'record.json')
        argv = [sys.executable, RUNNER, '--exe', exe,
                '--exe-arg', '10000', '--exe-arg', '1000',
                '--build-dir', build, '--fixture-id', 'bench_1m_relations',
                '--corpus-version', 'seed:42/99999', '--min-samples', '30',
                '--phase', 'insert', '--phase', 'query',
                '--out', out] + extra
        p = subprocess.run(argv, capture_output=True, text=True, timeout=300)
        check('runner rc', p.returncode == 0, p.stderr.strip()[:200])
        if p.returncode != 0:
            return 1
        rec = json.load(open(out, encoding='utf-8'))
        check('complete', rec['complete'] is True, str(rec['limitations']))
        check('fixture id', rec['fixture']['id'] == 'bench_1m_relations')
        check('both phases', set(rec['phases']) == {'insert', 'query'},
              str(set(rec['phases'])))
        for name in ('insert', 'query'):
            st = rec['phases'][name]
            check('%s samples' % name, st['n'] >= 30, str(st['n']))
            check('%s ordering' % name,
                  st['min'] <= st['median'] <= st['max']
                  and st['min'] <= st['p99'] <= st['max'])
            check('%s positive' % name, st['median'] > 0)
        check('memory positive', rec['memory']['peak_rss_bytes'] > 0,
              str(rec['memory']))
        check('subject sha', len(rec['subject_sha']) == 40)

    if FAILURES:
        print('FAILED: %d checks' % len(FAILURES))
        return 1
    print('all checks passed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
