#!/usr/bin/env python3
"""A native private flags mutant must exit42 AND report its exact failed guard."""
import subprocess
import sys
MARKER = 'ATOMIC_STORE_FLAGS_MUTANT missing_write_through=1'

def check(command, timeout=30):
    try:
        p = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           timeout=timeout)
        text = p.stdout.decode('utf-8', errors='strict')
    except (OSError, subprocess.TimeoutExpired, UnicodeError):
        return False
    return p.returncode == 42 and text.splitlines().count(MARKER) == 1

def selftest():
    def probe(body):
        return check([sys.executable, '-c', body], timeout=1)
    assert probe('print(' + repr(MARKER) + ');raise SystemExit(42)')
    for body in ('raise SystemExit(42)', 'print(' + repr(MARKER) + ')',
                 'print(' + repr(MARKER) + ');raise SystemExit(1)',
                 'print(' + repr(MARKER) + ');print(' + repr(MARKER) + ');raise SystemExit(42)',
                 'import time;time.sleep(2)',
                 'import sys;sys.stdout.buffer.write(bytes([255]));raise SystemExit(42)'):
        assert not probe(body)
    assert not check(['this-binary-does-not-exist'])
    return True

if __name__ == '__main__':
    ok = selftest() if sys.argv[1:] == ['--selftest'] else check(sys.argv[1:]) if sys.argv[1:] else False
    print('atomic_store_mutant_gate complete=' + str(int(ok)))
    sys.exit(0 if ok else 1)
