#!/usr/bin/env python3
"""Revision2 requires native exit0 AND the complete bounded evidence protocol."""
import argparse
import re
import sys
from shell_resource_gate import fields
import subprocess

FINAL = 'WINDOWS_SPAWN_V2 calls=27; persistent=1; exact_namedpipe_root=1; owner_unproven=1; candidate_not_closed=1; complete=1'
ARMS = ('observer_no_call', 'count_only_invalid', 'observed_invalid')


def validate(text, returncode):
    def need(ok, why):
        if not ok:
            raise ValueError(why)
    need(returncode == 0, 'native_exit')
    text.encode('utf-8', errors='strict')
    lines = [s.strip() for s in text.splitlines() if s.strip()]
    need(lines and lines[-1] == FINAL and lines.count(FINAL) == 1, 'final_marker')
    need(not any(s.startswith(('PSS_ERROR', 'PSS_OBSERVER')) for s in lines), 'PSS_error')
    need([s for s in lines if s.startswith('RESOURCE_CONTROLS ')] == ['RESOURCE_CONTROLS complete=1'], 'controls')
    need([s for s in lines if s.startswith('positive ')] == ['positive ok=1 exit=0 launch=0 timeout=0 failure_mask=5'], 'positive')
    need([s for s in lines if s.startswith('private runner mode=positive ')] == ['private runner mode=positive exit=0'], 'positive_exit')
    probes = [fields(s) for s in lines if s.startswith('SPAWN_PROBE ')]
    need(len(probes) == 2 and [s.get('kind') for s in probes] == ['1', '2'] and all(s.get('ok') == '0' and s.get('witness') == '0' and s.get('error') in ('193', '216') for s in probes), 'probes')
    arms = [fields(s) for s in lines if s.startswith('SPAWN_ARM ')]
    need([s.get('arm') for s in arms] == list(ARMS) and all(all(s.get(k) == v for k, v in {'complete': '1', 'calls': '27', 'child_exit': '0', 'cleanup': '1'}.items()) for s in arms), 'arms')
    checkpoints = [fields(s) for s in lines if s.startswith('SPAWN_CHECKPOINT ')]
    need([(s.get('arm'), s.get('call')) for s in checkpoints] == [(a, str(i)) for a in ARMS for i in range(28)], 'checkpoint_order')
    bases = {}
    for a in arms:
        group = [s for s in checkpoints if s['arm'] == a['arm']]
        need(all(s.get('count', '').isdigit() for s in group), 'count_format')
        baseline = int(group[0]['count']); bases[a['arm']] = baseline
        need(baseline > 0 and a.get('baseline') == str(baseline) and a.get('final') == group[-1]['count'], 'baseline_summary')
        need([int(s['count']) - baseline for s in group] == [0] + [0 if a['arm'] == ARMS[0] else 1] * 27, 'exact_deltas')
    results = [fields(s) for s in lines if s.startswith('SPAWN_RESULT ')]
    need([(s.get('arm'), s.get('call')) for s in results] == [(a, str(i)) for a in ARMS[1:] for i in range(1, 28)], 'result_order')
    result_want = {'mask': '0', 'ok': '0', 'exit': '-1', 'launch': '1', 'timeout': '0', 'stored': '0/0', 'total': '0/0', 'truncated': '0/0', 'first': '0/0', 'witness': '0'}
    need(all(all(s.get(k) == v for k, v in result_want.items()) for s in results), 'result_fields')
    tables = [fields(s) for s in lines if s.startswith('SPAWN_TABLE ')]
    need([s.get('call') for s in tables] == [str(i) for i in range(1, 28)], 'table_order')
    candidates = set()
    baseline = bases[ARMS[2]]
    for s in tables:
        need(all(s.get(k) == v for k, v in {'baseline': str(baseline), 'entries': str(baseline + 1), 'baseline_stable': '1', 'extras': '1', 'type': 'File', 'candidate_stable': '1'}.items()), 'table_identity')
        need(s.get('type_bytes') in ('8', '10') and s.get('flags', '').isdigit() and int(s['flags']) & 1 and re.fullmatch(r'0x[0-9a-f]+', s.get('candidate', '')) and int(s['candidate'], 16) != 0, 'candidate_type')
        candidates.add(s['candidate'])
    need(len(candidates) == 1, 'candidate_changed')
    candidate = next(iter(candidates))
    resources = [fields(s) for s in lines if s.startswith('SPAWN_RESOURCE ')]
    need([s.get('call') for s in resources] == ['1', '27'] and all(all(s.get(k) == v for k, v in {'candidate': candidate, 'exact_nt_root': '1', 'file_type': '3', 'target_stable': '1', 'candidate_not_closed': '1'}.items()) for s in resources), 'resources')
    queries = [(i, s, fields(s)) for i, s in enumerate(lines) if s.startswith('RESOURCE_QUERY ') and fields(s).get('context', '').startswith('spawn_')]
    need([(s.get('context'), s.get('mode')) for _, _, s in queries] == [(f'spawn_call_{call}', str(mode)) for call in (1, 27) for mode in (0, 1)], 'query_order')
    query_caps = [(i, fields(s)) for i, s in enumerate(lines) if s.startswith('PSS_CAPTURE ') and fields(s).get('arm', '').startswith('spawn_query_')]
    need([(s.get('arm'), s.get('stage')) for _, s in query_caps] == [(a, str(call * 10 + mode)) for call in (1, 27) for mode in (0, 1) for a in ('spawn_query_before', 'spawn_query_after')], 'query_captures')
    for n, (index, line, s) in enumerate(queries):
        need(all(s.get(k) == v for k, v in {'attempted': '1', 'returned': '1', 'harness_error': '0', 'complete': '1', 'error': '0', 'pending': '0', 'timeout': '0', 'duplicate_error': '0', 'close': '1'}.items()), 'query_execution')
        if s['mode'] == '0':
            need(s.get('type') == '3' and s.get('raw_return') == '3' and s.get('name_chars') == '0' and line.endswith(' name=""'), 'pipe_type')
        else:
            need(s.get('name_chars') == '18' and s.get('raw_return') == '18' and line.endswith(' name="\\\\Device\\\\NamedPipe\\\\"'), 'exact_path')
        before = query_caps[n * 2][0]; after = query_caps[n * 2 + 1][0]
        need(before < index < after, 'query_interleave')
        helpers = [fields(t) for t in lines[before+1:index] if t.startswith('RESOURCE_HELPER ')]
        need(len(helpers) == 1 and all(helpers[0].get(k) == v for k, v in {'mode': s['mode'], 'timeout': '0', 'duplicate_error': '0', 'cleanup': '1'}.items()), 'helper')
    caps = [fields(s) for s in lines if s.startswith('PSS_CAPTURE ')]
    expected = []
    for arm in (ARMS[0], ARMS[2]):
        expected.extend([(arm, '0'), (arm, '0')])
        for call in range(1, 28):
            expected.append((arm, str(call)))
            if arm == ARMS[2] and call in (1, 27):
                expected.extend([(a, str(call * 10 + mode)) for mode in (0, 1) for a in ('spawn_query_before', 'spawn_query_after')])
    need([(s.get('arm'), s.get('stage')) for s in caps] == expected, 'capture_order')
    for s in caps:
        expect_count = bases[s['arm']] + (int(s['stage']) != 0) if s['arm'] == ARMS[2] else bases[ARMS[0]] if s['arm'] == ARMS[0] else baseline + 1
        need(all(s.get(k) == v for k, v in {'complete': '1', 'walk_status': '259', 'entries': str(expect_count), 'child_count': str(expect_count)}.items()), 'capture_fields')
    # Check phase interleaving too, not only each stream in isolation.
    for a in ARMS:
        cp = [i for i, s in enumerate(lines) if s.startswith('SPAWN_CHECKPOINT ') and fields(s).get('arm') == a]
        end = next(i for i, s in enumerate(lines) if s.startswith('SPAWN_ARM ') and fields(s).get('arm') == a)
        for call in range(1, 28):
            bound = cp[call+1] if call < 27 else end
            chunk = lines[cp[call]+1:bound]
            if a != ARMS[0]:
                need(len([s for s in chunk if s.startswith('SPAWN_RESULT ')]) == 1, 'result_phase')
            if a == ARMS[2]:
                need(len([s for s in chunk if s.startswith('SPAWN_TABLE ')]) == 1, 'table_phase')
                need(len([s for s in chunk if s.startswith('SPAWN_RESOURCE ')]) == (1 if call in (1, 27) else 0), 'resource_phase')
    return True


def run(command, timeout=180):
    try:
        p = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)
        text = p.stdout.decode('utf-8', errors='strict')
        print(text, end='' if text.endswith('\n') else '\n', flush=True)
        validate(text, p.returncode)
    except (OSError, subprocess.TimeoutExpired, UnicodeError, ValueError) as exc:
        if isinstance(exc, subprocess.TimeoutExpired) and exc.stdout:
            print(exc.stdout.decode('utf-8', errors='backslashreplace'), end='', flush=True)
        print('SPAWN_V2_GATE blocked=' + type(exc).__name__ + ':' + str(exc), file=sys.stderr)
        return 1
    print('SPAWN_V2_GATE native_exit_zero_and_complete_evidence=1')
    return 0


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ['--'] else args.command
    if not command:
        parser.error('native command required')
    sys.exit(run(command))
