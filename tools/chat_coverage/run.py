#!/usr/bin/env python3
"""Read-only serving measurement. Semantic misses are records, not a green gate."""
import argparse
import hashlib
import json
import pathlib
import subprocess
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
GOLD_SHA = {
    'gold-v3.json': 'a76c8e31e9ac61bd88bf554b8bdf536fd511a64f784a423479a5a82fb8d8bbe4',
    'gold-v3-dict.json': '55b04c8f1b3998720d759e507f71268f053fe1cc8d4f3c67e06c40395a5aaa7b',
    'gold-v4.json': '75e1383916cf16e1838e71fd0b1cfd31f431d05fe2c0bcfb74600f73c8cbee3f',
    'gold-v4-dict.json': '706ab087c299ac42ed255f393c8a11c2764e74051da83ad125d2ec3c2e9fd1a7',
    'gold-v5.json': '0bfcbb8259071bec94ebc6a64bad9386f89fd812a0e11cd126c5d3902c8bcb8e',
    'gold-v5-dict.json': '1e75a6634fb2b3cefdedc87f642108fa8dd9e40ed8ebe1240a38c4c7344702a3',
}

def validate(result, expected_cells):
    rows = result['rows']
    assert len(rows) == expected_cells, 'incomplete matrix'
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

def load_gold(gold_path):
    assert hashlib.sha256(gold_path.read_bytes()).hexdigest() == GOLD_SHA[gold_path.name], 'gold changed'
    gold = json.loads(gold_path.read_text())
    if 'dict_fixture' in gold:
        d = HERE / gold['dict_fixture']['path']
        assert hashlib.sha256(d.read_bytes()).hexdigest() == gold['dict_fixture']['sha256'], 'dict fixture changed'
    return gold

def expected_cells(gold):
    return 2 * len(gold['cases']) * len(gold['modes']) * len(gold['capacities'])

def measure(probe, cwd, gold_path):
    gold = load_gold(gold_path)
    groups = [('fresh', c['id'], [c]) for c in gold['cases']]
    groups += [('stateful', d, [c for c in gold['cases'] if c['corpus'] == d + '.txt']) for d in ['memory', 'astronomy', 'physics', 'math', 'cocina', 'taller', 'enmemory']]
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
    return validate({'status': 'TECHNICAL_MEASUREMENT_GREEN', 'semantic_scoring': 'not_certified', 'gold_sha256': GOLD_SHA[gold_path.name], 'base_of_original_measurement': gold['base'], 'cells': len(rows), 'rows': rows}, expected_cells(gold))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=pathlib.Path)
    parser.add_argument('--cwd', type=pathlib.Path, default=None,
                        help='Working directory for the probe; default is an empty temp dir so no '
                             'cwd-relative dictionary/embeddings leak into the measurement')
    parser.add_argument('--replay', type=pathlib.Path, help='Validate an existing raw matrix; does not execute the engine')
    parser.add_argument('--gold', type=pathlib.Path, default=HERE / 'gold-v5.json', help='Frozen gold file (SHA-pinned)')
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    if args.replay:
        result = validate(json.loads(args.replay.read_text()), expected_cells(load_gold(args.gold)))
    elif args.probe:
        cwd = args.cwd.resolve() if args.cwd else pathlib.Path(tempfile.mkdtemp(prefix='chat-coverage-cwd-'))
        result = measure(args.probe.resolve(), cwd, args.gold)
    else:
        parser.error('choose --probe or --replay')
    if args.output.exists():
        parser.error('output exists; use a new evidence filename')
    args.output.write_text(json.dumps(result, indent=2))
    print('Technical integrity checked; semantic support NOT certified. Cells:', result['cells'])
