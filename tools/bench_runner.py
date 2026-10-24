#!/usr/bin/env python3
"""Reproducible benchmark runner (M3 criterion 5). Design: docs/m3-c5-benchmark-runner-design.md.

Spawns a benchmark executable once, parses `SAMPLE <phase> <ns>` lines plus a
final `BENCH_DONE`, measures the child's memory high-water from outside, and
writes a strict JSON record: exact keys, bounded ASCII text, no bool as int,
no nonfinite numbers. Fail-closed: any contract breach, missing environment
field or nonzero child exit refuses (exit 2, no record), unless
--allow-incomplete writes a diagnostic record marked complete:false, which is
never claim input. The runner performs no network, build or solver actions;
it measures only its own child process.
"""
import argparse
import ctypes
import datetime
import hashlib
import json
import math
import os
import platform
import re
import statistics
import subprocess
import sys

PHASE = re.compile(r'[a-z][a-z0-9_]{0,31}\Z')
SAMPLE = re.compile(r'SAMPLE ([a-z][a-z0-9_]{0,31}) ([0-9]+)\Z')
PRINTABLE = re.compile(r'[ -~]+\Z')
DONE = b'BENCH_DONE'
MEM_METHODS = {'wait4-ru_maxrss', 'GetProcessMemoryInfo-PeakWorkingSetSize', 'unsupported'}


class Refuse(Exception):
    pass


def is_text(s, cap=160):
    return isinstance(s, str) and 0 < len(s) <= cap and PRINTABLE.fullmatch(s) is not None


def is_num(x):
    return (type(x) in (int, float) and type(x) is not bool
            and math.isfinite(x))


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def cpu_model():
    sysname = platform.system()
    if sysname == 'Linux':
        try:
            with open('/proc/cpuinfo', 'r', encoding='utf-8', errors='replace') as f:
                for line in f:
                    if line.startswith('model name'):
                        return line.split(':', 1)[1].strip()
        except OSError:
            pass
        return None
    if sysname == 'Darwin':
        try:
            out = subprocess.run(['sysctl', '-n', 'machdep.cpu.brand_string'],
                                 capture_output=True, text=True, timeout=10)
            if out.returncode == 0 and out.stdout.strip():
                return out.stdout.strip()
        except (OSError, subprocess.SubprocessError):
            pass
        return None
    if sysname == 'Windows':
        try:
            import winreg
            key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
                                 r'HARDWARE\DESCRIPTION\System\CentralProcessor\0')
            val, _ = winreg.QueryValueEx(key, 'ProcessorNameString')
            winreg.CloseKey(key)
            val = str(val).strip()
            return val or None
        except (OSError, ImportError):
            return None
    return None


def cmake_cache(build_dir):
    path = os.path.join(build_dir, 'CMakeCache.txt')
    out = {}
    try:
        with open(path, 'r', encoding='utf-8', errors='replace') as f:
            for line in f:
                m = re.match(r'([A-Za-z0-9_]+):[A-Z]+=(.*)\Z', line.rstrip('\n'))
                if m:
                    out[m.group(1)] = m.group(2)
    except OSError:
        return None
    return out


def cmake_compiler_id_version(build_dir):
    """Read compiler id+version from CMakeFiles/<ver>/CMakeCCompiler.cmake.

    Every generator writes it, including multi-config ones whose cache lacks
    CMAKE_C_COMPILER_ID/VERSION entries. Returns 'ID VERSION' or None.
    """
    import glob
    hits = sorted(glob.glob(os.path.join(build_dir, 'CMakeFiles', '*',
                                         'CMakeCCompiler.cmake')))
    if len(hits) != 1:
        return None
    cid = cver = None
    try:
        with open(hits[0], 'r', encoding='utf-8', errors='replace') as f:
            for line in f:
                m = re.match(r'set\(CMAKE_C_COMPILER_ID "([^"]+)"\)', line.strip())
                if m:
                    cid = m.group(1)
                m = re.match(r'set\(CMAKE_C_COMPILER_VERSION "([^"]+)"\)', line.strip())
                if m:
                    cver = m.group(1)
    except OSError:
        return None
    if cid and cver:
        return '%s %s' % (cid, cver)
    return None


def run_child(argv):
    """Run argv once. Return (rc, stdout_bytes, peak_rss_bytes, method).

    Memory is measured for exactly this child: Unix via os.wait4 rusage of
    the spawned pid, Windows via GetProcessMemoryInfo on the still-open
    process handle after exit. No other process pollutes the reading.
    """
    sysname = platform.system()
    if sysname in ('Linux', 'Darwin'):
        r, w = os.pipe()
        try:
            pid = os.posix_spawn(argv[0], argv, os.environ,
                                 file_actions=[(os.POSIX_SPAWN_DUP2, w, 1),
                                               (os.POSIX_SPAWN_CLOSE, r)])
        finally:
            os.close(w)
        chunks = []
        while True:
            block = os.read(r, 65536)
            if not block:
                break
            chunks.append(block)
        os.close(r)
        _, status, ru = os.wait4(pid, 0)
        if os.WIFEXITED(status):
            rc = os.WEXITSTATUS(status)
        else:
            rc = -int(os.WTERMSIG(status))
        peak = int(ru.ru_maxrss)
        if sysname == 'Linux':
            peak *= 1024
        return rc, b''.join(chunks), peak, 'wait4-ru_maxrss'
    if sysname == 'Windows':
        proc = subprocess.Popen(argv, stdout=subprocess.PIPE,
                                stderr=subprocess.DEVNULL)
        out, _ = proc.communicate()

        class COUNTERS(ctypes.Structure):
            _fields_ = [('cb', ctypes.c_ulong),
                        ('PageFaultCount', ctypes.c_ulong),
                        ('PeakWorkingSetSize', ctypes.c_size_t),
                        ('WorkingSetSize', ctypes.c_size_t),
                        ('QuotaPeakPagedPoolUsage', ctypes.c_size_t),
                        ('QuotaPagedPoolUsage', ctypes.c_size_t),
                        ('QuotaPeakNonPagedPoolUsage', ctypes.c_size_t),
                        ('QuotaNonPagedPoolUsage', ctypes.c_size_t),
                        ('PagefileUsage', ctypes.c_size_t),
                        ('PeakPagefileUsage', ctypes.c_size_t),
                        ('PrivateUsage', ctypes.c_size_t)]

        counters = COUNTERS()
        counters.cb = ctypes.sizeof(counters)
        ok = 0
        for lib in ('psapi', 'kernel32'):
            try:
                fn = getattr(ctypes.windll, lib).GetProcessMemoryInfo
                ok = fn(int(proc._handle), ctypes.byref(counters), counters.cb)
                if ok:
                    break
            except (AttributeError, OSError):
                continue
        if not ok:
            raise Refuse('GetProcessMemoryInfo failed')
        return proc.returncode, out, int(counters.PeakWorkingSetSize), \
            'GetProcessMemoryInfo-PeakWorkingSetSize'
    raise Refuse('unsupported platform: %s' % sysname)


def nearest_rank(sorted_samples, pct):
    n = len(sorted_samples)
    rank = int(math.ceil(pct / 100.0 * n))
    return sorted_samples[max(0, min(n - 1, rank - 1))]


def phase_stats(samples):
    s = sorted(samples)
    n = len(s)
    mean = statistics.fmean(s)
    return {'n': n, 'unit': 'ns', 'min': s[0], 'median': statistics.median(s),
            'p95': nearest_rank(s, 95), 'p99': nearest_rank(s, 99),
            'max': s[-1], 'mean': mean,
            'stdev': statistics.pstdev(s) if n > 1 else 0.0}


def parse_samples(raw, declared):
    body = raw.replace(b'\r\n', b'\n')
    if not (body == DONE or body.endswith(DONE + b'\n')):
        raise Refuse('BENCH_DONE terminator missing')
    samples = {}
    for ln in body.split(b'\n'):
        line = ln.decode('utf-8', errors='replace')
        m = SAMPLE.fullmatch(line)
        if m is None:
            if line.startswith('SAMPLE'):
                raise Refuse('malformed SAMPLE line: %r' % line[:80])
            continue
        phase, ns = m.group(1), int(m.group(2))
        if phase not in declared:
            raise Refuse('undeclared phase in output: %s' % phase)
        samples.setdefault(phase, []).append(ns)
    return samples


def validate_record(rec):
    """Strict structural validation; also applied to the runner's own output."""
    root = {'version', 'subject_sha', 'date_utc', 'command', 'executable_sha256',
            'fixture', 'repetitions', 'min_samples', 'environment', 'phases',
            'memory', 'complete', 'limitations'}
    if type(rec) is not dict or set(rec) != root:
        raise Refuse('record root keys')
    if rec['version'] != 1 or type(rec['version']) is bool:
        raise Refuse('record version')
    if not (isinstance(rec['subject_sha'], str)
            and re.fullmatch(r'[0-9a-f]{40}', rec['subject_sha'])):
        raise Refuse('record subject_sha')
    if not is_text(rec['date_utc'], 40):
        raise Refuse('record date_utc')
    if not (type(rec['command']) is list and 1 <= len(rec['command']) <= 64
            and all(is_text(c, 512) for c in rec['command'])):
        raise Refuse('record command')
    if not re.fullmatch(r'[0-9a-f]{64}', rec['executable_sha256'] or ''):
        raise Refuse('record executable_sha256')
    fx = rec['fixture']
    if type(fx) is not dict or set(fx) != {'id', 'corpus_version'} \
            or not is_text(fx['id'], 64) or not is_text(fx['corpus_version']):
        raise Refuse('record fixture')
    for k in ('repetitions', 'min_samples'):
        if type(rec[k]) is not int or rec[k] < 1:
            raise Refuse('record %s' % k)
    env = rec['environment']
    ekeys = {'os', 'os_release', 'arch', 'cpu', 'toolchain', 'build_type', 'ci'}
    if type(env) is not dict or set(env) != ekeys or not all(is_text(v) for v in env.values()):
        raise Refuse('record environment')
    if type(rec['phases']) is not dict or not rec['phases']:
        raise Refuse('record phases')
    skeys = {'n', 'unit', 'min', 'median', 'p95', 'p99', 'max', 'mean', 'stdev'}
    for name, st in rec['phases'].items():
        if PHASE.fullmatch(name) is None or type(st) is not dict or set(st) != skeys:
            raise Refuse('record phase %s' % name)
        if st['unit'] != 'ns' or type(st['n']) is not int or st['n'] < rec['min_samples']:
            raise Refuse('record phase %s n' % name)
        vals = [st[k] for k in ('min', 'median', 'p95', 'p99', 'max', 'mean', 'stdev')]
        if not all(is_num(v) for v in vals):
            raise Refuse('record phase %s numbers' % name)
        if not (st['min'] <= st['median'] <= st['max'] and st['min'] <= st['p95'] <= st['max']
                and st['min'] <= st['p99'] <= st['max'] and st['stdev'] >= 0):
            raise Refuse('record phase %s ordering' % name)
    mem = rec['memory']
    if type(mem) is not dict or set(mem) != {'peak_rss_bytes', 'method'} \
            or mem['method'] not in MEM_METHODS \
            or type(mem['peak_rss_bytes']) is not int or mem['peak_rss_bytes'] < 0:
        raise Refuse('record memory')
    if rec['complete'] is not True and rec['complete'] is not False:
        raise Refuse('record complete')
    if not (type(rec['limitations']) is list
            and all(is_text(x, 200) for x in rec['limitations'])):
        raise Refuse('record limitations')


def main(argv):
    ap = argparse.ArgumentParser(description='Symbols benchmark runner (M3 C5)')
    ap.add_argument('--exe', required=True)
    ap.add_argument('--exe-arg', action='append', default=[])
    ap.add_argument('--build-dir', required=True)
    ap.add_argument('--fixture-id', required=True)
    ap.add_argument('--corpus-version', required=True)
    ap.add_argument('--min-samples', type=int, default=30)
    ap.add_argument('--phase', action='append', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--toolchain', default=None)
    ap.add_argument('--build-type', default=None)
    ap.add_argument('--subject-sha', default=None)
    ap.add_argument('--allow-incomplete', action='store_true')
    args = ap.parse_args(argv)

    incomplete = []
    try:
        exe = os.path.abspath(args.exe)
        if not os.path.isfile(exe):
            raise Refuse('executable not found')
        if platform.system() != 'Windows' and not os.access(exe, os.X_OK):
            raise Refuse('executable not executable')
        declared = set(args.phase)
        for p in declared:
            if PHASE.fullmatch(p) is None:
                raise Refuse('bad --phase name: %r' % p)
        if not is_text(args.fixture_id, 64):
            raise Refuse('bad --fixture-id')
        if not is_text(args.corpus_version):
            raise Refuse('bad --corpus-version')
        if args.min_samples < 1:
            raise Refuse('bad --min-samples')

        cache = cmake_cache(args.build_dir)
        toolchain = args.toolchain
        build_type = args.build_type
        if cache is not None and toolchain is None:
            cid = cache.get('CMAKE_C_COMPILER_ID', '')
            cver = cache.get('CMAKE_C_COMPILER_VERSION', '')
            if cid and cver:
                toolchain = '%s %s' % (cid, cver)
        if toolchain is None:
            toolchain = cmake_compiler_id_version(args.build_dir)
        if toolchain is None and cache is not None:
            cc = cache.get('CMAKE_C_COMPILER', '')
            if cc:
                probe = subprocess.run([cc, '--version'], capture_output=True,
                                       text=True, timeout=30)
                if probe.returncode == 0 and probe.stdout.splitlines():
                    toolchain = probe.stdout.splitlines()[0].strip()
        if build_type is None and cache is not None:
            build_type = cache.get('CMAKE_BUILD_TYPE') or None
        if toolchain is None or not is_text(toolchain):
            raise Refuse('toolchain unknown (no CMakeCache and no --toolchain)')
        if build_type is None or not is_text(build_type):
            raise Refuse('build_type unknown (no CMakeCache and no --build-type)')

        subject = args.subject_sha
        if subject is None:
            probe = subprocess.run(['git', 'rev-parse', 'HEAD'],
                                   capture_output=True, text=True,
                                   cwd=os.path.dirname(os.path.abspath(__file__)))
            if probe.returncode != 0:
                raise Refuse('subject SHA unknown (not a git tree and no --subject-sha)')
            subject = probe.stdout.strip()
        if re.fullmatch(r'[0-9a-f]{40}', subject) is None:
            raise Refuse('bad subject SHA')

        cpu = cpu_model()
        if cpu is None or not is_text(cpu):
            cpu = 'unknown'
            incomplete.append('cpu model unreadable')

        cmd = [exe] + list(args.exe_arg)
        rc, raw, peak, method = run_child(cmd)
        if method == 'unsupported':
            incomplete.append('memory high-water unsupported on this platform')
            peak = 0
        if rc != 0:
            raise Refuse('benchmark exited %d' % rc)
        samples = parse_samples(raw, declared)
        phases = {}
        for phase in sorted(declared):
            got = samples.get(phase, [])
            if len(got) < args.min_samples:
                raise Refuse('phase %s has %d samples, min %d'
                             % (phase, len(got), args.min_samples))
            phases[phase] = phase_stats(got)

        if os.environ.get('GITHUB_ACTIONS') == 'true':
            ci = 'github-actions:%s' % os.environ.get('RUNNER_OS', 'unknown')
        else:
            ci = 'none'
        env = {'os': platform.system(), 'os_release': platform.release(),
               'arch': platform.machine(), 'cpu': cpu,
               'toolchain': toolchain, 'build_type': build_type, 'ci': ci}
        for k, v in env.items():
            if not is_text(v):
                raise Refuse('environment field %s unreadable or unsafe' % k)

        limitations = ['single machine, no CPU pinning or isolation',
                       'development measurement, not a latency guarantee',
                       'no comparative or baseline claim'] + incomplete
        rec = {'version': 1, 'subject_sha': subject,
               'date_utc': datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ'),
               'command': [os.path.relpath(os.path.abspath(__file__))] + argv,
               'executable_sha256': sha256_file(exe),
               'fixture': {'id': args.fixture_id, 'corpus_version': args.corpus_version},
               'repetitions': 1, 'min_samples': args.min_samples,
               'environment': env, 'phases': phases,
               'memory': {'peak_rss_bytes': peak, 'method': method},
               'complete': len(incomplete) == 0, 'limitations': limitations}
        if incomplete and not args.allow_incomplete:
            raise Refuse('incomplete environment: %s' % '; '.join(incomplete))
        validate_record(rec)
        payload = json.dumps(rec, indent=2, sort_keys=True, allow_nan=False) + '\n'
        if len(payload.encode('utf-8')) > 65536:
            raise Refuse('record exceeds 64 KiB')
        tmp = args.out + '.tmp'
        with open(tmp, 'w', encoding='utf-8', newline='\n') as f:
            f.write(payload)
        os.replace(tmp, args.out)
        print('record written: %s (complete=%s)' % (args.out, rec['complete']))
        return 0
    except Refuse as e:
        print('bench_runner: refused: %s' % e, file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
