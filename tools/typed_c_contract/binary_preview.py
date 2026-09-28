#!/usr/bin/env python3
"""Opt-in binary-safe, read-only static candidate preview. Never runs workspace C."""
import argparse
import difflib
import json
from pathlib import Path
import re
import sys

from schema_binary import parse
from preview import snapshot, source_checks, file_identity, sha, fail

SCHEMA = 'symbols.c-binary-preview.v1'
HEADER = b'#include <stdio.h>\n'
WIN = (b'#ifdef _WIN32\n#include <io.h>\n#include <fcntl.h>\n#endif\n')
MODE = (b'#ifdef _WIN32\n'
        b'if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;\n'
        b'#endif\n')
# Fullmatch over ASCII bytes, never tokenize an arbitrary C translation unit.
# Whitespace is permitted only between fixed tokens, not inside identifiers,
# the include target, or the literal. A required gap separates C keywords.
WS = rb'[ \t\r\n]*'
GAP = rb'[ \t\r\n]+'
INCLUDE = rb'[ \t]*\#include[ \t]+<stdio\.h>[ \t]*\r?\n'
MAIN = rb'int' + GAP + rb'main' + WS + rb'\(' + WS + rb'void' + WS + rb'\)' + WS + rb'\{'
END = rb'return' + GAP + rb'0' + WS + rb';' + WS + rb'\}' + WS
LITERAL = re.compile(WS + INCLUDE + WS + MAIN + WS +
                     rb'(printf|puts)' + WS + rb'\(' + WS + rb'"'
                     rb'((?:[\x20-\x21\x23-\x5b\x5d-\x7e]|\\[nrt"\\])*)'
                     rb'"' + WS + rb'\)' + WS + rb';' + WS + END)
EMPTY_SOURCE = re.compile(WS + INCLUDE + WS + MAIN + WS + END)
ELEMENTS = rb'0x[0-9a-f]{2}(?:,0x[0-9a-f]{2}){0,126}'
ARRAY = re.compile(rb'#include <stdio\.h>\n' + re.escape(WIN) +
                   rb'static const unsigned char B\[\] = \{(' + ELEMENTS + rb')\};\n'
                   rb'int main\(void\)\{\n' + re.escape(MODE) +
                   rb'fwrite\(B,1,sizeof\(B\),stdout\);\nreturn 0;\n\}\n')
EMPTY = HEADER + b'int main(void) { return 0; }\n'
ESCAPE = {ord('n'): 10, ord('r'): 13, ord('t'): 9, ord('"'): 34, ord('\\'): 92}


def literal_bytes(raw, call):
    out = bytearray()
    idx = 0
    while idx < len(raw):
        c = raw[idx]
        if c == 92:
            idx += 1
            c = ESCAPE[raw[idx]]
        out.append(c)
        idx += 1
    if call == b'puts':
        return bytes(out) + b'\n'
    # printf without conversion arguments. Only %% is supported.
    if b'%' in out.replace(b'%%', b''):
        return None
    return bytes(out).replace(b'%%', b'%')


def recognize(source, original=False):
    if original and (len(source) > 768 or len(source.splitlines()) > 32):
        return None
    if EMPTY_SOURCE.fullmatch(source):
        return b''
    match = LITERAL.fullmatch(source)
    if match:
        return literal_bytes(match.group(2), match.group(1))
    match = ARRAY.fullmatch(source)
    if match:
        return bytes(int(value, 16) for value in match.group(1).split(b','))
    return None


def generate(goal):
    if not goal:
        return EMPTY
    elements = b','.join(f'0x{value:02x}'.encode() for value in goal)
    return (HEADER + WIN + b'static const unsigned char B[] = {' + elements +
            b'};\nint main(void){\n' + MODE +
            b'fwrite(B,1,sizeof(B),stdout);\nreturn 0;\n}\n')


def validate(contract_file, root):
    contract_file = Path(contract_file); root = Path(root)
    if contract_file.is_symlink() or not contract_file.is_file(): fail('contract_file')
    if file_identity(contract_file, contract_file.stat())[0] != 1: fail('contract_link')
    if contract_file.stat().st_size > 8192: fail('contract_limit')
    if root.resolve() == contract_file.resolve() or root.resolve() in contract_file.resolve().parents:
        fail('contract_in_workspace')
    raw = contract_file.read_bytes()
    contract, goal, identity = parse(raw)
    files, digest = snapshot(root)
    if digest != contract['workspace_digest']: fail('workspace_digest')
    source_checks(files, contract)
    if len(files) != 1 or len(contract['edit_scope']['allow']) != 1:
        fail('no_eligible_source')
    path = contract['edit_scope']['allow'][0]
    if path in contract['edit_scope']['deny']: fail('scope_conflict')
    if path not in files or not path.endswith('.c'): fail('allow_path')
    before = files[path]
    current = recognize(before, original=True)
    candidates = []
    if current is not None and current != goal:
        after = generate(goal)
        if recognize(after) != goal: fail('candidate_bytes')
        changed = dict(files); changed[path] = after
        try:
            source_checks(changed, contract, files)
        except ValueError as exc:
            if str(exc) not in ('predicate_mismatch', 'span_changed', 'span_range'):
                raise
            after = None
        if after is not None:
            old_lines = before.decode('ascii').splitlines(keepends=True)
            new_lines = after.decode('ascii').splitlines(keepends=True)
            patch = ''.join(difflib.unified_diff(old_lines, new_lines,
                                                 fromfile='before/'+path, tofile='after/'+path, n=2))
            if len(patch.encode('utf-8')) > 4096: fail('candidate_patch_limit')
            candidates = [{'tier': 4, 'rule': 'answer_bytes', 'path': path,
                           'before_sha256': sha(before), 'after_sha256': sha(after),
                           'unified_diff': patch}]
    again, check = snapshot(root)
    if check != digest or again != files: fail('workspace_changed_during_preview')
    return {'schema': SCHEMA, 'status': 'static_candidate_unverified' if candidates else 'no_static_candidate',
            'promise': 'contract validated + static candidate preview; stdout NOT verified, nothing executed',
            'not_executed': True, 'provenance': 'operator-supplied/identity-unverified',
            'contract_sha256': identity, 'workspace_digest': digest,
            'candidate_count': len(candidates), 'candidates': candidates}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--preview-c-binary-contract', required=True)
    ap.add_argument('-w', '--workspace', required=True)
    args = ap.parse_args()
    try:
        result = validate(args.preview_c_binary_contract, args.workspace)
    except Exception as exc:
        code = str(exc) if isinstance(exc, ValueError) and str(exc) in {
            'limit','utf8_bom','duplicate_key','fields','version','digest','stdout','base64','exit',
            'probe','scope','predicates','path','span','predicate','contract_file','contract_link',
            'contract_limit','contract_in_workspace','workspace_root','file_identity','depth',
            'audit_dir','git_dir','path_limit','path_encoding','symlink','file_limit','binary_or_race',
            'total_limit','unsupported_entry','workspace_digest','workspace_changed_during_preview',
            'allow_path','deny_path','predicate_path','span_range','predicate_mismatch','span_changed',
            'no_eligible_source','scope_conflict','candidate_bytes','candidate_patch_limit'} else 'invalid_or_unavailable'
        sys.stderr.buffer.write(b'refused: ' + code.encode('ascii') + b'\n')
        return 2
    sys.stdout.buffer.write((json.dumps(result, sort_keys=True, separators=(',', ':')) + '\n').encode('utf-8'))
    return 0

if __name__ == '__main__':
    sys.exit(main())
