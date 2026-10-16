#!/usr/bin/env python3
"""Frozen pre-code arity cases plus real environment/caller execution gates."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parents[1]
gold = json.loads((root / 'tests/compiler_environment_E_gold.json').read_text())
completed = []

def run(args, text=None, env=None, timeout=90):
    # Diagnostic fixtures are byte-defined UTF-8, including curly quotes.
    # Windows' default ANSI encoding would turn U+2018/U+2019 into 91/92.
    # Other child/tool output keeps the platform encoding it already used.
    result = subprocess.run([str(binary), *args], input=text, text=True,
                            encoding='utf-8' if text is not None else None,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            env=env, timeout=timeout)
    if result.returncode:
        raise AssertionError((args, result.returncode, result.stdout, result.stderr))
    return result.stdout

def parse(text):
    lines = run(['--parse'], text).splitlines()
    header = list(map(int, lines.pop(0).split()))
    items = []
    for _ in range(header[1]):
        fields = list(map(int, lines.pop(0).split()))
        items.append((fields, lines.pop(0), lines.pop(0), lines.pop(0)))
    return header, items, '\n'.join(lines)

for case in gold['cases']:
    if case['route'] != 'parser' or 'expected_token' not in case:
        continue
    expected, have = case['expected_token'], case['have_token']
    fields = []
    if expected is not None:
        fields.append(('' if case['id'] == 'key_inside_word' else 'expected ') + expected)
    if have is not None:
        fields.append('have ' + have)
    text = "arity.c:7:2: error: too few arguments to function 'foo'; " + ' '.join(fields) + '\n'
    header, items, remedy = parse(text)
    valid = case['expected']['type'] == 'ARITY_MISMATCH'
    assert header == [1, 1, 1, 0, 3 if valid else 0], (case['id'], header)
    values = [int(re.match(r'\d+', token).group()) for token in (expected, have)] if valid else [0, 0]
    assert items[0][0] == [3 if valid else 0, *values, 7, 2, 0], (case['id'], items)
    assert items[0][1:3] == ('arity.c', 'foo'), (case['id'], items)
    assert items[0][3].startswith("too few arguments to function 'foo';"), (case['id'], items)
    if valid:
        assert f'expected {values[0]} arguments, provided {values[1]}.' in remedy, (case['id'], remedy)
    else:
        assert not re.search(r'expected [0-9]+ arguments, provided [0-9]+', remedy), (case['id'], remedy)
        assert remedy.startswith('Compilation failure '), (case['id'], remedy)
    completed.append(case['id'])

warning = "warn.c:1:1: warning: too many arguments to function 'warn'; expected 19, have 23\n"
error = "arity.c:7:2: error: too few arguments to function 'foo'; expected 7, have 2\n"
h, items, remedy = parse(warning + error)
assert h == [1, 2, 1, 1, 3] and 'expected 7 arguments, provided 2.' in remedy and '19' not in remedy
completed.append('warning_precedes_error')
h, items, remedy = parse(error.replace('expected 7', 'expected -7') + error)
assert h == [1, 2, 2, 0, 0] and 'provided' not in remedy
completed.append('invalid_arity_then_valid_error')
for quote in ["'", '\u2018']:
    close = "'" if quote == "'" else '\u2019'
    text = f'm.c:3:4: error: {quote}obj{close} has no member named {quote}bad{close}; did you mean {quote}good{close}?\n'
    h, items, remedy = parse(text)
    assert h == [1, 1, 1, 0, 2] and items[0][2] == 'bad' and "'good'" in remedy
completed.append('nonarity_and_quotes')
run(['--inconsistent'])

env = dict(os.environ)
# Seed inside a separate test process. Production code must never change these.
for key in list(env):
    if key.upper() in {'LC_ALL', 'LC_MESSAGES', 'LANG', 'VSLANG', 'SYMBOLS_ALLOW_DESTRUCTIVE'}:
        del env[key]
env.update(LC_ALL='C.utf8', LC_MESSAGES='parent-message', LANG='parent-language',
           VSLANG='1049', SYMBOLS_E_SENTINEL='kept value')
if os.name == 'nt':
    # Windows special current-directory entry travels in the native block.
    env['=C:'] = 'C:\\'
    env['lC_mEsSaGeS'] = env['LC_MESSAGES']
command = binary.name + ' --env-child' if os.name == 'nt' else './' + binary.name + ' --env-child'
output = run(['--environment', str(binary.parent), command], env=env)
assert output == 'environment_failures=0\n', output
run(['--runner-route', str(binary.parent), binary.name if os.name == 'nt' else './' + binary.name], env=env)
completed += ['compiler_child_env', 'normal_and_test_inherit', 'concurrent_normal_compiler',
              'allocation_failure', 'guarded_refusal', 'failure_timeout_cleanup']
if os.name == 'nt':
    completed.append('windows_env_case_duplicates')

# A real toolchain emission, with explicit evidence of what ran. No translated
# output is claimed from a fake locale or from a toolchain lacking catalogs.
compiler = shutil.which('cl') if os.name == 'nt' else (shutil.which('gcc') or shutil.which('clang'))
if os.name == 'nt' and not compiler:
    candidates = sorted(Path(os.environ.get('ProgramFiles', 'C:/Program Files')).glob(
        'Microsoft Visual Studio/*/*/VC/Tools/MSVC/*/bin/Hostx64/x64/cl.exe'))
    if candidates:
        compiler = str(candidates[-1])
        env['PATH'] = str(Path(compiler).parent) + os.pathsep + env['PATH']
assert compiler, 'native compiler unavailable for E emission check'
with tempfile.TemporaryDirectory(prefix='symbols-E-compiler-') as directory:
    Path(directory, 'broken.c').write_text('int main(void) { return symbols_E_undefined; }\n')
    command = 'cl /nologo /c broken.c' if os.name == 'nt' else Path(compiler).name + ' -fsyntax-only broken.c'
    output = run(['--compiler', directory, command], env=env)
    head, diagnostic = output.split('\n', 1)
    state = list(map(int, head.split()[1:]))
    assert state[0] == 1 and state[1] != 0 and state[2:] == [0, 0], output
    assert 'error' in diagnostic and 'symbols_E_undefined' in diagnostic, output
    parsed, _, _ = parse(diagnostic)
    assert parsed[2] >= 1, (output, parsed)
    print('ACTUAL_COMPILER', compiler, head)
    print(diagnostic.strip())
completed.append('actual_compiler')

# Routing is explicit rather than substring heuristics. Keep this source guard
# alongside functional existing TaskOps and Runner suites. No runtime command
# rewriting is introduced by this inventory check.
source = (root / 'src/task_ops.c').read_text()
assert source.count('AgentShellExecCompiler(cmd,') == 13, source.count('AgentShellExecCompiler(cmd,')
assert 'AgentShellExec(cmd, dir, 5000, r)' in source
runner = (root / 'src/agent_runner.c').read_text()
assert 'AgentShellExecCompilerGuarded(task->build_command,' in runner
assert 'AgentShellExecGuarded(task->test_command,' in runner
completed.append('build_routes_pinned')

# Native v2 must remain a real runtime gate, not a source-only assurance. Each
# native configuration executes it here as well as the dedicated CTest entry.
if os.name == 'nt':
    frozen = root / 'tests/shell_spawn_v2_gate.py'
    native = binary.parent / 'test_shell_spawn_win.exe'
    result = subprocess.run([sys.executable, str(frozen), '--', str(native)],
                            text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=195)
    print(result.stdout)
    assert result.returncode == 0, (result.returncode, result.stderr)
    completed += ['windows_v2_release_unchanged', 'windows_v2_asan_unchanged']
    # The CI configuration identifies whether this particular execution is
    # Release or ASAN. These IDs express the shared test, not two executions.
    print('NATIVE_V2 this configuration executed; Release AND ASAN CI required')
else:
    print('NATIVE_WINDOWS_ONLY windows_env_case_duplicates windows_v2_release_unchanged windows_v2_asan_unchanged')
all_ids = {case['id'] for case in gold['cases']}
assert set(completed) <= all_ids
expected = all_ids if os.name == 'nt' else all_ids - {'windows_env_case_duplicates','windows_v2_release_unchanged','windows_v2_asan_unchanged'}
assert set(completed) == expected, sorted(expected - set(completed))
print('E_GOLD_EXECUTED', len(set(completed)), 'of', len(all_ids), 'defined case IDs')
