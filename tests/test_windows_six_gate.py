#!/usr/bin/env python3
"""Frozen positive/negative vectors and real subprocess error controls."""
import contextlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import windows_six_gate as gate

GOLD = json.loads(Path(__file__).with_name('windows_six_gate_gold.json').read_text(encoding='utf-8'))


def check(ok, name):
    if not ok:
        raise AssertionError(name)


def main():
    executed = []
    with tempfile.TemporaryDirectory(prefix='six-gate-selftest-') as temp:
        temp = Path(temp)
        child = temp / 'child.py'
        child.write_text('''import pathlib,sys,time
raw=pathlib.Path(sys.argv[1]).read_bytes()
stream=sys.stderr.buffer if sys.argv[3]=='stderr' else sys.stdout.buffer
stream.write(raw);stream.flush()
if sys.argv[3]=='timeout':time.sleep(30)
sys.exit(int(sys.argv[2]))
''', encoding='utf-8')
        for vector in GOLD['vectors']:
            kind, name = vector['kind'], vector['id']
            framing = '\r\n' if vector.get('framing') == 'CRLF' else '\n'
            raw = (framing.join(vector['lines']) + framing).encode('utf-8')
            if vector.get('encoding') == 'invalid_utf8':
                raw += b'\xff'
            failure = vector.get('failure')
            if failure:
                payload = temp / 'payload.bin'
                payload.write_bytes(raw)
                if failure == 'OSError':
                    command = [str(temp / 'nonexistent-executable')]
                else:
                    command = [sys.executable, str(child), str(payload), '0', 'timeout']
                with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                    result = gate.run(kind, command, timeout=0.15)
                accepted = result == 0
            else:
                try:
                    gate.validate(kind, raw, vector['exit'])
                    accepted = True
                except (ValueError, UnicodeError):
                    accepted = False
            check(accepted == vector['valid'], name)
            executed.append(name)
        check(len(executed) == GOLD['vector_count'] and len(set(executed)) == len(executed), 'vector_coverage')
        # Derived from each frozen full line: omitting any one semicolon group
        # must fail, even if every other group remains correct.
        omissions = 0
        for kind, oracle in GOLD['gates'].items():
            groups = oracle['final_line'].split(';')
            for missing in range(len(groups)):
                partial = ';'.join(groups[:missing] + groups[missing + 1:])
                raw = ('\n'.join(oracle.get('arm_summaries', []) + [partial]) + '\n').encode('utf-8')
                try:
                    gate.validate(kind, raw, 0)
                except (ValueError, UnicodeError):
                    omissions += 1
                else:
                    raise AssertionError(kind + '_omitted_group_' + str(missing))
        # Real wrapper subprocesses, not synthetic return-code validation only.
        for kind, oracle in GOLD['gates'].items():
            payload = temp / (kind + '.bin')
            payload.write_bytes(('\n'.join(oracle.get('arm_summaries', []) + [oracle['final_line']]) + '\n').encode('utf-8'))
            for code in (0, 1):
                p = subprocess.run([sys.executable, str(Path(gate.__file__)), '--kind', kind, '--',
                                    sys.executable, str(child), str(payload), str(code), 'stdout'],
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=15)
                check(p.returncode == code, kind + '_actual_exit_' + str(code))
                check((b'native_exit_zero_and_full_oracle=1' in p.stdout) == (code == 0), kind + '_actual_gate_marker')
            # A stderr fragment is still incomplete when streams are merged.
            payload.write_bytes(oracle['final_line'].split(';')[0].encode('utf-8') + b'\n')
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                result = gate.run(kind, [sys.executable, str(child), str(payload), '0', 'stderr'])
            check(result == 1, kind + '_stderr_fragment')
        # Slice H3 (2026-10-17): the native binary prints a diagnostic line to stderr ahead of the
        # final line (merged into the gate stream); the gate must accept it for both sweep kinds.
        diag_child = temp / 'diag_child.py'
        diag_child.write_text('''import pathlib,sys
sys.stderr.write("sweep foreign diag argc=2 mutant_env=13"+chr(10));sys.stderr.flush()
sys.stdout.write(pathlib.Path(sys.argv[1]).read_text(encoding='utf-8'))
''', encoding='utf-8')
        for kind in ('sweep_foreign', 'sweep_foreign_mc13'):
            payload = temp / (kind + '.diag.bin')
            payload.write_bytes(('\n'.join(GOLD['gates'][kind].get('arm_summaries', []) + [GOLD['gates'][kind]['final_line']]) + '\n').encode('utf-8'))
            eout, eerr = io.StringIO(), io.StringIO()
            with contextlib.redirect_stdout(eout), contextlib.redirect_stderr(eerr):
                result = gate.run(kind, [sys.executable, str(diag_child), str(payload)])
            check(result == 0, kind + '_diag_line_accepted ' + eerr.getvalue()[:160])
    print('WINDOWS_SIX_SELFTEST vectors=' + str(len(executed)) + ' derived_omissions=' + str(omissions) + ' subprocess_controls=23 complete=1')
    return 0


if __name__ == '__main__':
    sys.exit(main())
