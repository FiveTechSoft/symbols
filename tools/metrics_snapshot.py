#!/usr/bin/env python3
"""Render curated evidence only. No measurement, network or solver actions."""
import argparse
import datetime
import json
import os
from pathlib import Path
import re
import stat
import tempfile

SHA = re.compile(r'[0-9a-f]{40}\Z')
URL = re.compile(r'https://github\.com/FiveTechSoft/symbols/(?:actions/runs/[0-9]+|commit/[0-9a-f]{40})\Z')
KINDS = {'technical_result', 'developer_contract', 'historical_efficacy', 'unknown'}
BEGIN = b'<!-- METRICS:BEGIN -->'
END = b'<!-- METRICS:END -->'


def require(ok, reason):
    if not ok:
        raise ValueError(reason)


def text(s, cap):
    return (isinstance(s, str) and 0 < len(s) <= cap and
            all(32 <= ord(c) < 127 and c not in '|`<>&\\' for c in s))


def unique(pairs):
    out = {}
    for k, v in pairs:
        require(k not in out, 'duplicate JSON key')
        out[k] = v
    return out


def load(raw):
    require(len(raw) <= 65536, 'snapshot too large')
    obj = json.loads(raw.decode('utf-8'), object_pairs_hook=unique,
                     parse_constant=lambda s: (_ for _ in ()).throw(ValueError('nonfinite JSON')))
    require(type(obj) is dict and set(obj) == {'version', 'subject_sha', 'entries'}, 'root fields')
    require(type(obj['version']) is int and obj['version'] == 1, 'version')
    require(isinstance(obj['subject_sha'], str) and SHA.fullmatch(obj['subject_sha']), 'subject SHA')
    require(type(obj['entries']) is list and 1 <= len(obj['entries']) <= 32, 'entry count')
    seen = set()
    for e in obj['entries']:
        require(type(e) is dict and set(e) == {'id', 'title', 'claim_type', 'measured_sha', 'date', 'environment', 'source_url', 'value'}, 'entry fields')
        require(isinstance(e['id'], str) and re.fullmatch('[a-z][a-z0-9_]{0,63}', e['id']) and e['id'] not in seen, 'entry id')
        seen.add(e['id'])
        require(text(e['title'], 100) and text(e['environment'], 160), 'entry text')
        require(isinstance(e['claim_type'], str) and e['claim_type'] in KINDS, 'claim type')
        require(isinstance(e['date'], str) and re.fullmatch('[0-9]{4}-[0-9]{2}-[0-9]{2}', e['date']), 'date')
        datetime.date.fromisoformat(e['date'])
        require(isinstance(e['source_url'], str) and URL.fullmatch(e['source_url']), 'source URL')
        if e['claim_type'] == 'unknown':
            require(e['value'] is None and e['measured_sha'] is None, 'unknown value')
        else:
            require(text(e['value'], 400), 'value')
            require(isinstance(e['measured_sha'], str) and SHA.fullmatch(e['measured_sha']), 'measured SHA')
    return obj


def render(obj):
    lines = ['Evidence snapshot for subject commit `' + obj['subject_sha'] + '`.',
             '', 'Curated source claims, not new measurements. CI success is not semantic efficacy.', '',
             '| Metric | Evidence class | Value | Measurement context | Source |',
             '|---|---|---|---|---|']
    for e in obj['entries']:
        kind = e['claim_type']
        if kind == 'unknown':
            label, value, sha = 'Unmeasured on subject SHA', 'Unmeasured', obj['subject_sha']
        else:
            if kind == 'historical_efficacy':
                label = 'Historical efficacy'
            else:
                label = ('Current ' if e['measured_sha'] == obj['subject_sha'] else 'Historical ') + ('technical' if kind == 'technical_result' else 'developer')
            value, sha = e['value'], e['measured_sha']
        context = e['date'] + '; ' + e['environment'] + '; `' + sha + '`'
        lines.append('| ' + ' | '.join([e['title'], label, value, context, '[source](' + e['source_url'] + ')']) + ' |')
    return '\n'.join(lines) + '\n'


def rewrite(path, block):
    path = Path(path)
    st = path.lstat()
    require(stat.S_ISREG(st.st_mode), 'README must be regular, not symlink')
    raw = path.read_bytes()
    require(b'\0' not in raw, 'NUL README')
    lines = raw.splitlines(keepends=True)
    starts, ends, offset = [], [], 0
    for line in lines:
        # BOM is preserved outside the marker region, not stripped from lines.
        if line.rstrip(b'\r\n') == BEGIN:
            starts.append((offset, len(line), line[len(BEGIN):]))
        if line.rstrip(b'\r\n') == END:
            ends.append((offset, len(line), line[len(END):]))
        offset += len(line)
    require(len(starts) == len(ends) == 1, 'unique standalone markers required')
    first, n, eol = starts[0]
    last, _, end_eol = ends[0]
    require(first < last and eol in (b'\n', b'\r\n') and end_eol == eol, 'marker order/newlines')
    # Reject hidden marker copies, including nonstandalone occurrences.
    require(raw.count(BEGIN) == raw.count(END) == 1, 'duplicate markers')
    updated = raw[:first+n] + block.replace('\n', eol.decode()).encode('ascii') + raw[last:]
    tmp = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix='.metrics-', delete=False) as f:
            tmp = Path(f.name)
            f.write(updated)
        os.chmod(tmp, stat.S_IMODE(st.st_mode))
        os.replace(tmp, path)
    finally:
        if tmp is not None and tmp.exists():
            tmp.unlink()


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--snapshot', required=True)
    p.add_argument('--readme')
    p.add_argument('--write', action='store_true')
    args = p.parse_args(argv)
    try:
        if args.write:
            require(args.readme is not None, '--write requires --readme')
        obj = load(Path(args.snapshot).read_bytes())
        block = render(obj)
        if args.write:
            rewrite(args.readme, block)
        else:
            print(block, end='')
    except (ValueError, OSError, UnicodeError) as e:
        p.exit(2, 'metrics snapshot: ' + str(e) + '\n')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
