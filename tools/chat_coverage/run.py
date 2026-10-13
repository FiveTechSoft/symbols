#!/usr/bin/env python3
"""Read-only serving measurement. Semantic misses are records, not a green gate."""
import argparse
import hashlib
import json
import pathlib
import subprocess
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
GOLD_SHA = '75d771bd383cf4bba050981e0e3d1bb1fa08b68c7e3bb545c40021ce41b630f1'

def validate(result):
    rows = result['rows']
    assert len(rows) == 192, 'incomplete matrix'
    keys = {(r['id'], r['arm'], r['mode'], r['capacity']) for r in rows}
    assert len(keys) == len(rows), 'duplicate cell'
    for r in rows:
        q = r['record']
        if q['receipt_state'] == 1:
            hits = [e for e in q['evidence'] if e['file'] == q['receipt_file'] and e['sentence'] == q['receipt_sentence']]
            assert hits and hits[-1]['valid'], 'invalid receipt'
            assert (q['receipt_first'], q['receipt_end']) == (hits[-1]['first'], hits[-1]['end']), 'wrong source span'
        if r['capacity'] == 32:
            wide = next(a['record'] for a in rows if a['id'] == r['id'] and a['arm'] == r['arm'] and a['mode'] == r['mode'] and a['capacity'] == 4096)
            assert bytes.fromhex(q['out_hex']) == bytes.fromhex(wide['out_hex'])[:31], 'presentation changed'
            for k in ['status', 'cause', 'shown', 'tnw']:
                assert q[k] == wide[k], 'decision/state changed'
    return result

def measure(probe, cwd):
    assert hashlib.sha256((HERE / 'gold.json').read_bytes()).hexdigest() == GOLD_SHA, 'gold changed'
    gold = json.loads((HERE / 'gold.json').read_text())
    groups = [('fresh', c['id'], [c]) for c in gold['cases']]
    groups += [('stateful', d, [c for c in gold['cases'] if c['corpus'] == d + '.txt']) for d in ['memory', 'astronomy', 'physics', 'math']]
    rows = []
    with tempfile.TemporaryDirectory(prefix='chat-coverage-') as td:
        query_file = pathlib.Path(td) / 'queries.txt'
        for arm, gid, cases in groups:
            for c in cases:
                assert hashlib.sha256((HERE / 'fixtures' / c['corpus']).read_bytes()).hexdigest() == c['source_sha256'], 'source changed'
            query_file.write_text(''.join(c['query'] + '\n' for c in cases))
            for mode in gold['modes']:
                for cap in gold['capacities']:
                    reps = []
                    for rep in range(gold['repetitions']):
                        call = subprocess.run([str(probe), str(HERE / 'fixtures' / cases[0]['corpus']), str(query_file), str(cap), mode], cwd=cwd, text=True, capture_output=True, timeout=1200)
                        if call.returncode:
                            raise RuntimeError((gid, mode, cap, rep, call.returncode, call.stderr))
                        lines = [l for l in call.stdout.splitlines() if l.startswith('CHAT_ACCEPTANCE_RESULT=')]
                        assert len(lines) == 1, 'missing result'
                        reps.append(json.loads(lines[0].split('=', 1)[1])['queries'])
                    assert reps[0] == reps[1], 'nondeterministic'
                    assert len(reps[0]) == len(cases), 'query count'
                    for c, q in zip(cases, reps[0]):
                        rows.append({'id': c['id'], 'positive': c['positive'], 'arm': arm, 'mode': mode, 'capacity': cap, 'record': q})
    return validate({'status': 'TECHNICAL_MEASUREMENT_GREEN', 'semantic_scoring': 'not_certified', 'gold_sha256': GOLD_SHA, 'base_of_original_measurement': gold['base'], 'cells': len(rows), 'rows': rows})

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=pathlib.Path)
    parser.add_argument('--cwd', type=pathlib.Path, default=pathlib.Path.cwd())
    parser.add_argument('--replay', type=pathlib.Path, help='Validate an existing raw matrix; does not execute the engine')
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    if args.replay:
        result = validate(json.loads(args.replay.read_text()))
    elif args.probe:
        result = measure(args.probe.resolve(), args.cwd.resolve())
    else:
        parser.error('choose --probe or --replay')
    if args.output.exists():
        parser.error('output exists; use a new evidence filename')
    args.output.write_text(json.dumps(result, indent=2))
    print('Technical integrity checked; semantic support NOT certified. Cells:', result['cells'])
