#!/usr/bin/env python3
"""Six native Windows gates: exit zero AND the complete frozen output line."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

GOLD = json.loads(Path(__file__).with_name('windows_six_gate_gold.json').read_text(encoding='utf-8'))
LIMITS = {'dual_win': 170, 'snapshot_win': 110, 'aliases_short': 285,
          'lock_alias': 285, 'sweep_foreign': 285, 'batch_replace': 285}


def validate(kind, output, returncode):
    if kind not in GOLD['gates']:
        raise ValueError('unknown_kind')
    if returncode != 0:
        raise ValueError('native_exit_nonzero')
    if not isinstance(output, bytes):
        raise ValueError('bytes_required')
    text = output.decode('utf-8', errors='strict')
    text = text.replace('\r\n', '\n')
    if '\r' in text or '\0' in text:
        raise ValueError('invalid_line_framing')
    lines = [line for line in text.split('\n') if line != '']
    oracle = GOLD['gates'][kind]
    final = oracle['final_line']
    if not lines or lines[-1] != final or lines.count(final) != 1:
        raise ValueError('unique_complete_final_line')
    if kind == 'snapshot_win':
        arms = [line for line in lines if line.startswith('PSS_ARM ')]
        if arms != oracle['arm_summaries']:
            raise ValueError('snapshot_arm_summaries')
    return True


def emit(raw):
    # Preserve diagnostics even when strict validation rejects their encoding.
    text = raw.decode('utf-8', errors='backslashreplace')
    print(text, end='' if text.endswith('\n') else '\n', flush=True)


def run(kind, command, timeout=None):
    try:
        p = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           timeout=LIMITS[kind] if timeout is None else timeout)
        emit(p.stdout)
        validate(kind, p.stdout, p.returncode)
    except (OSError, subprocess.TimeoutExpired, UnicodeError, ValueError) as exc:
        if isinstance(exc, subprocess.TimeoutExpired) and exc.stdout:
            emit(exc.stdout)
        print('WINDOWS_SIX_GATE kind=' + kind + ' blocked=' + type(exc).__name__ + ':' + str(exc),
              file=sys.stderr, flush=True)
        return 1
    print('WINDOWS_SIX_GATE kind=' + kind + ' native_exit_zero_and_full_oracle=1', flush=True)
    return 0


def main():
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='strict')
        sys.stderr.reconfigure(encoding='utf-8', errors='strict')
    parser = argparse.ArgumentParser()
    parser.add_argument('--kind', required=True, choices=sorted(GOLD['gates']))
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command
    if command and command[0] == '--':
        command = command[1:]
    if not command:
        parser.error('native command required')
    return run(args.kind, command)


if __name__ == '__main__':
    sys.exit(main())
