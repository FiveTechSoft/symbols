#!/usr/bin/env python3
"""Opt-in, read-only one-TU impact comparison; never authorizes edits."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

from importlib.util import module_from_spec, spec_from_file_location

# Do not rely on `inspect` import-name shadowing against the stdlib.
_spec = spec_from_file_location("ast_pilot", Path(__file__).with_name("inspect.py"))
_pilot = module_from_spec(_spec)
_spec.loader.exec_module(_pilot)
inspect_tu = _pilot.inspect


def unknown(reason, **details):
    return {"status": "unknown", "reason": reason, **details}


def within(loc, extent):
    point = (loc['line'], loc['column'])
    start = (extent['start']['line'], extent['start']['column'])
    end = (extent['end']['line'], extent['end']['column'])
    return start <= point < end


def compare(ast, lexical, symbol, target_file, target_line):
    if ast.get('status') != 'complete':
        return unknown('ast_' + ast.get('reason', 'unavailable'), ast_status=ast.get('status'))
    source = ast['source']
    target = str(Path(target_file).resolve())
    if target != source:
        return unknown('target_outside_selected_source', source=source)
    definitions = [f for f in ast['facts'] if f['kind'] == 'declaration' and f['definition']
                   and f['name'] == symbol and f['location']['file'] == target
                   and f['location']['line'] == target_line and f['type'].endswith(')')]
    if len(definitions) != 1:
        return unknown('target_definition_not_unique', matches=len(definitions), source=source)
    definition = definitions[0]
    target_loc = definition['location']
    functions = [f for f in ast['facts'] if f['kind'] == 'declaration' and f['definition']
                 and f['location']['file'] == source and f['type'].endswith(')')]
    callers, references = {}, []
    for fact in ast['facts']:
        if fact['kind'] not in ('call', 'reference') or fact.get('target') != target_loc:
            continue
        if fact['kind'] == 'reference':
            references.append(fact['location'])
            continue
        owners = [f for f in functions if within(fact['location'], f['span'])]
        if len(owners) != 1:
            return unknown('call_owner_not_unique', location=fact['location'])
        owner = owners[0]
        key = owner['name']
        if key in callers and callers[key]['definition'] != owner['location']:
            return unknown('duplicate_caller_name', name=key)
        callers.setdefault(key, {'definition': owner['location'], 'call_sites': []})['call_sites'].append(fact['location'])
    lexical_names = set(lexical['direct_callers'])
    ast_names = set(callers)
    tp = len(ast_names & lexical_names)
    return {'status': 'complete', 'scope': 'selected_tu_direct_calls_only', 'symbol': symbol,
            'target': target_loc, 'source': source, 'variant': ast['variant'],
            'provenance': {k: ast[k] for k in ('command_sha256', 'db_sha256', 'source_sha256', 'includes', 'libclang_version')},
            'ast_callers': [dict(name=name, **callers[name]) for name in sorted(callers)],
            'ast_references': references, 'lexical_caller_names': sorted(lexical_names),
            'ast_only': sorted(ast_names - lexical_names), 'lexical_only': sorted(lexical_names - ast_names),
            'agreement_on_names': ast_names == lexical_names,
            'selected_tu_name_metrics': {'true_positive': tp, 'lexical_only': len(lexical_names - ast_names),
                                         'ast_only': len(ast_names - lexical_names),
                                         'precision': tp / len(lexical_names) if lexical_names else None,
                                         'recall': tp / len(ast_names) if ast_names else None},
            'limitations': ['read_only', 'no_edit_authority', 'disagreement_not_resolved',
                            'single_tu_not_project_complete', 'lexical_names_not_bindings',
                            'lexical_references_unavailable']}


def run(db, source, symbol, line, variant, lexical_bin):
    src = str(Path(source).resolve())
    dbpath = Path(db).resolve()
    try:
        before = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in (Path(src), dbpath)}
    except OSError:
        return unknown('missing_source_or_db')
    # The pilot prints exactly one JSON object and returns 2 for unknown.
    # Capture it in-process without ever accepting a partial AST as complete.
    import contextlib
    import io
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        rc = inspect_tu(str(dbpath), src, variant)
    try:
        ast = json.loads(out.getvalue())
    except ValueError:
        return unknown('invalid_ast_output')
    if rc != 0 or ast.get('status') != 'complete':
        return unknown('ast_' + ast.get('reason', 'unavailable'), ast_status=ast.get('status'))
    try:
        p = subprocess.run([str(Path(lexical_bin).resolve()), src, symbol],
                           capture_output=True, text=True, timeout=30, check=False)
        lexical = json.loads(p.stdout) if p.returncode == 0 else None
        if not isinstance(lexical, dict) or lexical.get('symbol') != symbol or lexical.get('source') != src \
                or not isinstance(lexical.get('direct_callers'), list) \
                or not all(isinstance(name, str) for name in lexical['direct_callers']):
            lexical = None
    except (OSError, ValueError, subprocess.TimeoutExpired):
        lexical = None
    if lexical is None:
        return unknown('lexical_graph_unavailable', source=src)
    try:
        if any(hashlib.sha256(p.read_bytes()).hexdigest() != digest for p, digest in before.items()):
            return unknown('inputs_changed_during_comparison')
        if hashlib.sha256(Path(src).read_bytes()).hexdigest() != ast['source_sha256']:
            return unknown('inspector_source_hash_mismatch')
        if any(hashlib.sha256(Path(item['file']).read_bytes()).hexdigest() != item['sha256']
               for item in ast['includes']):
            return unknown('includes_changed_during_comparison')
    except OSError:
        return unknown('inputs_changed_during_comparison')
    return compare(ast, lexical, symbol, src, line)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--db', required=True)
    p.add_argument('--source', required=True)
    p.add_argument('--symbol', required=True)
    p.add_argument('--line', type=int, required=True, help='exact definition line in selected C source')
    p.add_argument('--variant', type=int)
    p.add_argument('--lexical-bin', required=True, help='built lexical_impact executable')
    a = p.parse_args()
    r = run(a.db, a.source, a.symbol, a.line, a.variant, a.lexical_bin)
    print(json.dumps(r, sort_keys=True))
    return 0 if r['status'] == 'complete' else 2


if __name__ == '__main__':
    sys.exit(main())
