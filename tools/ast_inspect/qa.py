#!/usr/bin/env python3
"""Explicit, read-only C source-fact query over one pinned libclang TU."""
import argparse
from contextlib import redirect_stdout
import io
import json
import re
import sys

from inspect import inspect


def answer(db, source, variant, fact, symbol):
    if not re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]{0,127}', symbol, re.ASCII):
        return {'status': 'unknown', 'reason': 'invalid_symbol'}
    stream = io.StringIO()
    with redirect_stdout(stream):
        rc = inspect(db, source, variant)
    try:
        observed = json.loads(stream.getvalue())
    except (ValueError, TypeError):
        return {'status': 'unknown', 'reason': 'inspector_failed'}
    if rc != 0 or observed.get('status') != 'complete':
        return {'status': 'unknown', 'reason': observed.get('reason') or 'inspector_failed'}
    try:
        facts = observed['facts']
        provenance = {k: observed[k] for k in ('source', 'source_sha256', 'db_sha256',
                                               'command_sha256', 'includes')}
    except (KeyError, TypeError):
        return {'status': 'unknown', 'reason': 'invalid_inspector_result'}
    # Name alone cannot choose between a local, global, prototype, or shadow.
    declarations = [f for f in facts if f['kind'] == 'declaration' and f['name'] == symbol]
    if len(declarations) != 1:
        return {'status': 'unknown', 'reason': 'symbol_absent_or_ambiguous'}
    declaration = declarations[0]
    location = declaration['location']
    if fact == 'declaration':
        selected = [declaration]
    else:
        kind = 'call' if fact == 'calls' else 'reference'
        selected = [f for f in facts if f['kind'] == kind and f.get('target') == location]
    # No unchecked argument, source text or checker content is echoed. Empty
    # reference/call lists are facts about this one complete TU only.
    return {'status': 'complete', 'fact': fact, 'symbol': symbol,
            'scope': 'selected_translation_unit_source_facts_only',
            **provenance, 'declaration': declaration,
            'matches': selected}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--db', required=True)
    p.add_argument('--source', required=True)
    p.add_argument('--variant', type=int)
    p.add_argument('--fact', required=True, choices=('declaration', 'references', 'calls'))
    p.add_argument('--symbol', required=True)
    a = p.parse_args()
    response = answer(a.db, a.source, a.variant, a.fact, a.symbol)
    print(json.dumps(response, sort_keys=True))
    return 0 if response['status'] == 'complete' else 2


if __name__ == '__main__':
    sys.exit(main())
