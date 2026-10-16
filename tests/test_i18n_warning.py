#!/usr/bin/env python3
"""Narrow warning regression and fixed lookup gold. No whole-project claim."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile


def checked(command, **kwargs):
    result = subprocess.run(command, capture_output=True, text=True, timeout=60, **kwargs)
    if result.returncode:
        raise RuntimeError('%r exit=%d\n%s%s' % (command, result.returncode, result.stdout, result.stderr))
    return result.stdout


def source_contract(root):
    for path, ending in [('include/i18n.h', ';'), ('src/i18n.c', '{')]:
        text = (root/path).read_text(encoding='utf-8')
        # Narrow source-format contract, independently guards scalar return on MSVC.
        pattern = r'(?m)^LANG_ID LangFindByCode\(const char \*code\)\s*' + re.escape(ending)
        if len(re.findall(pattern, text)) != 1 or 'const LANG_ID LangFindByCode(' in text:
            raise RuntimeError('scalar return declaration/definition contract: '+path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True)
    parser.add_argument('--compiler-id', required=True)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    root = args.root.resolve()
    source_contract(root)
    gold = json.loads((root/'tests/i18n_warning_gold.json').read_text())
    cases = []
    for name, words in gold['valid'].items():
        for word in words:
            cases.append((word, 'LANG_'+name))
    cases += [(word, 'LANG_COUNT') for word in gold['unknown']]
    body = ['#include "i18n.h"', 'int main(void) {', '  int state;',
            '  for (state = LANG_EN; state < LANG_COUNT; ++state) {', '    LangSet((LANG_ID)state);']
    for index, (word, expected) in enumerate(cases, 1):
        literal = '0' if word is None else json.dumps(word)
        body += ['    if (LangFindByCode(%s) != %s) return %d;' % (literal, expected, index),
                 '    if (LangGet() != (LANG_ID)state) return %d;' % (index+len(cases))]
    body += ['  }', '  return 0;', '}']
    with tempfile.TemporaryDirectory(prefix='symbols-i18n-warning-') as directory:
        tmp = Path(directory)
        (tmp/'lookup.c').write_text('\n'.join(body)+'\n', encoding='utf-8')
        (tmp/'header.c').write_text('#include "i18n.h"\nint check(void) { return LangFindByCode("EN") == LANG_EN; }\n', encoding='utf-8')
        if args.compiler_id == 'MSVC':
            common = [args.compiler, '/nologo', '/W4', '/WX', '/TC', '/I'+str(root/'include')]
            checked(common+['/c', str(tmp/'header.c'), '/Fo'+str(tmp/'header.obj')], cwd=tmp)
            checked(common+['/Dstrcasecmp=_stricmp', str(tmp/'lookup.c'), str(root/'src/i18n.c'),
                            '/Fe'+str(tmp/'lookup.exe')], cwd=tmp)
            executable = tmp/'lookup.exe'
        elif args.compiler_id in ('GNU', 'Clang', 'AppleClang'):
            common = [args.compiler, '-Wall', '-Wextra', '-Werror', '-I'+str(root/'include')]
            checked(common+['-std=c11', '-c', str(tmp/'header.c'), '-o', str(tmp/'header.o')], cwd=tmp)
            # GNU dialect matches project extensions, not an ISO-C11 source-clean claim.
            checked(common+['-std=gnu11', str(tmp/'lookup.c'), str(root/'src/i18n.c'), '-o', str(tmp/'lookup')], cwd=tmp)
            executable = tmp/'lookup'
        else:
            raise RuntimeError('unreviewed compiler '+args.compiler_id)
        checked([str(executable)], cwd=tmp)
    print('i18n warning regression PASS compiler=%s cases=%d states=3' % (args.compiler_id, len(cases)))


if __name__ == '__main__':
    main()
