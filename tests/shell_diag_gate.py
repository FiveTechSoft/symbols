#!/usr/bin/env python3
"""Strict execution/trace gates for private Windows diagnostics, not ownership proof."""
import argparse
import re
import subprocess
import sys


class GateError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise GateError(message)


class Trace:
    def __init__(self, lines):
        self.lines = iter(lines)
        self.position = 0

    def take(self, pattern):
        self.position += 1
        line = next(self.lines, None)
        match = re.fullmatch(pattern, line) if line is not None else None
        require(match is not None, 'line %d: expected %s, got %r' %
                (self.position, pattern, line))
        return match.groups()

    def end(self):
        require(next(self.lines, None) is None, 'extra transcript lines')


APIS = ('GetFileAttributesA', 'QueryPerformanceFrequency',
        'QueryPerformanceCounter', 'CreatePipe', 'SetHandleInformation',
        'CreatePipe', 'SetHandleInformation', 'CreateJobObjectA',
        'SetInformationJobObject', 'GetStdHandle', 'CreateProcessA') + ('CloseHandle',) * 5


def boundary(t, call, before):
    records = []
    for index, api in enumerate(APIS):
        fields = t.take(r'SHELL_BOUNDARY call=%d index=%d api=%s before=(\d+) after=(\d+) delta=(-?\d+) success=([01]) last_error=(\d+) h1=0x([0-9a-fA-F]+) h2=0x([0-9a-fA-F]+)' % (call, index, api))
        b, a, d, ok, error = map(int, fields[:5])
        h1, h2 = (int(v, 16) for v in fields[5:])
        require(b == (records[-1][1] if records else before), 'boundary count discontinuity')
        require(d == a - b, 'boundary delta mismatch')
        require(ok == (0 if api == 'CreateProcessA' else 1), 'boundary API result')
        if api == 'CreateProcessA':
            require(error in (193, 216), 'boundary launch error')
        records.append((b, a, d, h1, h2))
    owned = [records[3][3], records[3][4], records[5][3], records[5][4], records[7][3]]
    borrowed = records[9][3]
    require(len(set(owned)) == 5 and all(owned), 'owned handles must be five distinct nonzero values')
    require(borrowed not in owned, 'borrowed stdin owned/closed')
    require(records[4][3] == owned[0] and records[6][3] == owned[2] and
            records[8][3] == owned[4], 'API handle relationship')
    for index in (0, 1, 2, 10):
        require(records[index][3:] == (0, 0), 'unexpected API output handles')
    for index in (4, 6, 7, 8, 9, 11, 12, 13, 14, 15):
        require(records[index][4] == 0, 'unexpected second handle')
    require([r[3] for r in records[11:]] == owned, 'owned close order/identity')
    for h in owned:
        got, = t.take(r'SHELL_LEDGER call=%d owned=0x([0-9a-fA-F]+) successful_closes=1' % call)
        require(int(got, 16) == h, 'ledger handle mismatch')
    h, summed, outer = t.take(r'SHELL_LEDGER call=%d borrowed_stdin=0x([0-9a-fA-F]+) owned_count=5 sum_delta=(-?\d+) outer_delta=(-?\d+) complete=1' % call)
    delta = records[-1][1] - before
    require(int(h, 16) == borrowed and int(summed) == sum(r[2] for r in records) == int(outer) == delta,
            'ledger outer/sum/borrowed mismatch')
    return records[-1][1]


def child(t, mode, prefix, instrumented=False):
    error_mode, = t.take(r'private child entered mode=%s error_mode=(\d+) critical_dialogs_disabled=1' % mode)
    require(int(error_mode) & 1, 'critical dialog error mode missing')
    if mode.endswith('positive'):
        t.take(r'positive ok=1 exit=0 launch=0 timeout=0 failure_mask=5')
        return
    t.take(r'invalid-image probe before CreateProcessA')
    t.take(r'invalid-image native probe ok=0 error=(193|216)')
    t.take(r'handle probe warmed; native failure calls=27')
    value, = t.take(prefix + r' baseline=(\d+) calls=27 blocks=3,12,12')
    baseline = int(value)
    previous = baseline
    counts = []
    for call in range(1, 28):
        block = 1 if call <= 3 else 2 if call <= 15 else 3
        after = boundary(t, call, previous) if instrumented else None
        value, = t.take(prefix + r' call=%d block=%d count=(\d+) mask=0 ok=0 exit=-1 launch=1 timeout=0 stored=0/0 total=0/0' % (call, block))
        count = int(value)
        require(after is None or after == count, 'outer count mismatch')
        counts.append(count)
        previous = count
        if call in (3, 15, 27):
            value, delta = t.take(prefix + r' block_end=%d count=(\d+) delta=(-?\d+)' % block)
            require(int(value) == count and int(delta) == count - baseline, 'block end mismatch')
    flat = all(c == baseline for c in counts)
    one_time = all(c == baseline + 1 for c in counts)
    require(flat or one_time, 'unexpected handle count progression')
    arm = ' arm=' + mode if prefix == 'OWNER_HANDLE_TRACE' else ''
    classification, b, final, delta = t.take(prefix + arm + r' classification=(flat|one_time_increase) baseline=(\d+) final=(\d+) delta=(-?\d+) result_masks_ok=1')
    require(classification == ('flat' if flat else 'one_time_increase') and
            int(b) == baseline and int(final) == counts[-1] and int(delta) == counts[-1] - baseline,
            'classification mismatch')


def validate(kind, stdout, stderr, code):
    if code != 0:
        raise GateError('diagnostic exit %s' % code)
    require(stderr == '', 'unexpected diagnostic stderr')
    require(isinstance(stdout, str), 'stdout is not text')
    lines = stdout.splitlines()
    modes = ('positive', 'invalid') if kind == 'handles' else (
        'normal_positive', 'normal_invalid', 'instrumented_positive', 'instrumented_invalid')
    terminal = ('WINDOWS_SPAWN_HANDLE_DIAG calls=27; counts=28; sensitivity mask=5' if kind == 'handles'
                else 'WINDOWS_SPAWN_OWNER_DIAG arms=2; calls_per_arm=27; sensitivity mask=5')
    require(lines and lines[-1] == terminal, 'missing full terminal marker')
    # Parent stdio may be buffered behind child output. Only full, ordered exit rows move.
    exits = [line for line in lines if line.startswith('private runner ')]
    require(exits == ['private runner mode=%s exit=0' % mode for mode in modes], 'runner exits missing/duplicate/failed/order')
    t = Trace([line for line in lines[:-1] if not line.startswith('private runner ')])
    for mode in modes:
        child(t, mode, 'SPAWN_HANDLE_TRACE' if kind == 'handles' else 'OWNER_HANDLE_TRACE',
              mode == 'instrumented_invalid')
    t.end()


def run(kind, command, timeout=None):
    timeout = timeout if timeout is not None else (80 if kind == 'handles' else 170)
    try:
        result = subprocess.run(command, capture_output=True, text=True, encoding='utf-8',
                                errors='strict', shell=False, timeout=timeout)
    except (OSError, UnicodeError, subprocess.TimeoutExpired) as exc:
        raise GateError('diagnostic launch/decode/timeout: %s' % exc) from exc
    sys.stdout.write(result.stdout)
    sys.stderr.write(result.stderr)
    validate(kind, result.stdout, result.stderr, result.returncode)


def fixture(kind, increase=0, interspersed=False):
    """Source-derived synthetic trace. No native values/addresses are required."""
    lines = []
    modes = ('positive', 'invalid') if kind == 'handles' else (
        'normal_positive', 'normal_invalid', 'instrumented_positive', 'instrumented_invalid')
    prefix = 'SPAWN_HANDLE_TRACE' if kind == 'handles' else 'OWNER_HANDLE_TRACE'
    for mode in modes:
        lines.append('private child entered mode=%s error_mode=1 critical_dialogs_disabled=1' % mode)
        if mode.endswith('positive'):
            lines.append('positive ok=1 exit=0 launch=0 timeout=0 failure_mask=5')
        else:
            lines += ['invalid-image probe before CreateProcessA', 'invalid-image native probe ok=0 error=193',
                      'handle probe warmed; native failure calls=27', prefix + ' baseline=10 calls=27 blocks=3,12,12']
            previous = 10
            for call in range(1, 28):
                block = 1 if call <= 3 else 2 if call <= 15 else 3
                if mode == 'instrumented_invalid':
                    current = previous
                    hs = [(0, 0)] * 3 + [(0x100, 0x104), (0x100, 0), (0x108, 0x10c),
                         (0x108, 0), (0x110, 0), (0x110, 0), (0x90, 0), (0, 0)] + [
                             (h, 0) for h in (0x100, 0x104, 0x108, 0x10c, 0x110)]
                    for index, api in enumerate(APIS):
                        d = (2 + (increase if call == 1 else 0)) if index == 3 else 2 if index == 5 else 1 if index == 7 else -1 if index >= 11 else 0
                        lines.append('SHELL_BOUNDARY call=%d index=%d api=%s before=%d after=%d delta=%d success=%d last_error=%d h1=0x%x h2=0x%x' %
                                     (call, index, api, current, current+d, d, 0 if index == 10 else 1, 193 if index >= 10 else 0, *hs[index]))
                        current += d
                    for h in (0x100, 0x104, 0x108, 0x10c, 0x110):
                        lines.append('SHELL_LEDGER call=%d owned=0x%x successful_closes=1' % (call, h))
                    lines.append('SHELL_LEDGER call=%d borrowed_stdin=0x90 owned_count=5 sum_delta=%d outer_delta=%d complete=1' % (call, current-previous, current-previous))
                count = 10 + increase
                lines.append(prefix + ' call=%d block=%d count=%d mask=0 ok=0 exit=-1 launch=1 timeout=0 stored=0/0 total=0/0' % (call, block, count))
                if call in (3, 15, 27):
                    lines.append(prefix + ' block_end=%d count=%d delta=%d' % (block, count, increase))
                previous = count
            arm = ' arm=' + mode if kind == 'owner' else ''
            lines.append(prefix + arm + ' classification=%s baseline=10 final=%d delta=%d result_masks_ok=1' %
                         ('one_time_increase' if increase else 'flat', 10+increase, increase))
        if interspersed:
            lines.append('private runner mode=%s exit=0' % mode)
    if not interspersed:
        lines += ['private runner mode=%s exit=0' % mode for mode in modes]
    lines.append('WINDOWS_SPAWN_HANDLE_DIAG calls=27; counts=28; sensitivity mask=5' if kind == 'handles'
                 else 'WINDOWS_SPAWN_OWNER_DIAG arms=2; calls_per_arm=27; sensitivity mask=5')
    return '\n'.join(lines) + '\n'


def selftest():
    controls = 0
    for kind in ('handles', 'owner'):
        for increase in (0, 1):
            for interspersed in (False, True):
                validate(kind, fixture(kind, increase, interspersed), '', 0)
                controls += 1
    negatives = []
    h, o = fixture('handles', 1), fixture('owner', 1)
    def add(name, kind, text, err='', code=0):
        negatives.append((name, kind, text, err, code))
    for kind, text in (('handles', h), ('owner', o)):
        add(kind+' exit1', kind, text, code=1)
        add(kind+' partial terminal', kind, text.rsplit('; sensitivity mask=5', 1)[0]+'\n')
        add(kind+' terminal only', kind, text.splitlines()[-1]+'\n')
        add(kind+' stderr', kind, text, 'FATAL\n')
        add(kind+' unknown row', kind, 'unrelated\n'+text)
        add(kind+' truncated', kind, text[:len(text)//2])
        for label, before, after in [
            ('mask', 'mask=0 ok=0', 'mask=1 ok=0'), ('result', 'stored=0/0', 'stored=1/0'),
            ('control', 'failure_mask=5', 'failure_mask=0'), ('critical dialogs', 'error_mode=1', 'error_mode=0'),
            ('block', 'call=4 block=2', 'call=4 block=1'), ('count drift', 'call=2 block=1 count=11', 'call=2 block=1 count=12'),
            ('classification', 'classification=one_time_increase', 'classification=flat'),
            ('block delta', 'block_end=1 count=11 delta=1', 'block_end=1 count=11 delta=0'),
            ('missing call', ' call=2 block=1', ' call=3 block=1'),
            ('probe error', 'native probe ok=0 error=193', 'native probe ok=0 error=2'),
            ('runner duplicate', 'private runner mode=', 'private runner mode=invalid exit=0\nprivate runner mode=')]:
            require(before in text, 'negative setup '+label)
            add(kind+' '+label, kind, text.replace(before, after, 1))
        rows = text.splitlines(True)
        index = next(i for i, row in enumerate(rows) if ' call=1 block=1 ' in row)
        add(kind+' deleted call', kind, ''.join(rows[:index]+rows[index+1:]))
        add(kind+' duplicated call', kind, ''.join(rows[:index]+[rows[index]]+rows[index:]))
        changed = rows[:]
        # Adjacent call rows for handles; owner has full boundary blocks between calls.
        other = next(i for i,row in enumerate(rows) if ' call=2 block=1 ' in row)
        changed[index], changed[other] = changed[other], changed[index]
        add(kind+' call order', kind, ''.join(changed))
    for label, before, after in [
        ('arm missing', 'mode=instrumented_positive', 'mode=normal_positive'),
        ('ledger incomplete', 'complete=1', 'complete=0'),
        ('owned count', 'owned_count=5', 'owned_count=4'),
        ('ledger handle', 'owned=0x100', 'owned=0x110'),
        ('duplicate close', 'api=CloseHandle before=15 after=14 delta=-1 success=1 last_error=193 h1=0x104',
         'api=CloseHandle before=15 after=14 delta=-1 success=1 last_error=193 h1=0x100'),
        ('borrowed closed', 'h1=0x90 h2=0x0', 'h1=0x100 h2=0x0'),
        ('API', 'api=GetFileAttributesA', 'api=UnknownAPI'),
        ('index', 'index=1 api=', 'index=0 api='),
        ('boundary call', 'SHELL_BOUNDARY call=1 index=0', 'SHELL_BOUNDARY call=2 index=0'),
        ('boundary delta', 'delta=0 success=1', 'delta=1 success=1'),
        ('continuity', 'before=10 after=10', 'before=9 after=9'),
        ('sum', 'sum_delta=1', 'sum_delta=2'),
        ('outer', 'outer_delta=1', 'outer_delta=0'),
        ('close failed', 'api=CloseHandle before=16 after=15 delta=-1 success=1', 'api=CloseHandle before=16 after=15 delta=-1 success=0')]:
        require(before in o, 'negative setup '+label)
        add('owner '+label, 'owner', o.replace(before, after, 1))
    for name, kind, text, err, code in negatives:
        try:
            validate(kind, text, err, code)
        except GateError:
            continue
        raise GateError('negative accepted: '+name)
    import contextlib
    import io
    import tempfile
    from pathlib import Path
    # Exercise the real subprocess path, including exit/timeout/launch failure.
    with tempfile.TemporaryDirectory() as tmp:
        file = Path(tmp)/'producer.py'
        file.write_text('import sys\nsys.stdout.write(%r)\nsys.exit(int(sys.argv[1]))\n' % h, encoding='utf-8')
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            run('handles', [sys.executable, str(file), '0'])
            controls += 1
            for command, timeout in [([sys.executable, str(file), '1'], 5),
                                     ([sys.executable, '-c', 'import time; time.sleep(5)'], .05),
                                     ([str(Path(tmp)/'missing.exe')], 5)]:
                try:
                    run('handles', command, timeout)
                except GateError:
                    continue
                raise GateError('subprocess negative accepted')
    print('shell_diag_gate selftest controls=%d negatives=%d PASS' % (controls, len(negatives)+3))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--kind', choices=('handles', 'owner'))
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    try:
        if args.self_test:
            selftest()
        else:
            require(args.kind is not None and args.command, '--kind and executable required')
            command = args.command[1:] if args.command[0] == '--' else args.command
            require(command, 'executable required')
            run(args.kind, command)
            print('shell_diag_gate kind=%s PASS' % args.kind)
    except GateError as exc:
        print('shell_diag_gate FAIL: %s' % exc, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
