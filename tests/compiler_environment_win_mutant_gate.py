#!/usr/bin/env python3
"""Native VSLANG mutation: exact exit42 AND actual-child/parent witness."""
import os
from pathlib import Path
import subprocess
import sys

def validate(exitcode, stdout, stderr):
    expected = b'E_VSLANG_MUTANT actual_child_inherited1049_parent1049=1\r\n'
    return exitcode == 42 and stdout in (expected, expected.replace(b'\r\n', b'\n')) and not stderr

if len(sys.argv) == 2 and sys.argv[1] == '--selftest':
    marker = b'E_VSLANG_MUTANT actual_child_inherited1049_parent1049=1\n'
    assert validate(42, marker, b'')
    for exitcode, stdout, stderr in [(0,marker,b''),(1,marker,b''),(42,marker*2,b''),
                                    (42,b'PASS\n',b''),(42,b'\xff',b''),(42,marker,b'error')]:
        assert not validate(exitcode, stdout, stderr)
    raise SystemExit(0)

binary = Path(sys.argv[1]).resolve()
env = dict(os.environ)
for key in list(env):
    if key.upper() == 'VSLANG': del env[key]
env['VSLANG'] = '1049'
try:
    result = subprocess.run([str(binary),'--vslang-proof',str(binary.parent),binary.name+' --env-child'],
                            env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=25)
except (OSError, subprocess.TimeoutExpired) as exc:
    print(type(exc).__name__, file=sys.stderr)
    raise SystemExit(1)
sys.stdout.buffer.write(result.stdout)
sys.stderr.buffer.write(result.stderr)
raise SystemExit(0 if validate(result.returncode,result.stdout,result.stderr) else 1)
