#!/usr/bin/env python3
"""Strict full-i18n TU compilation and frozen differential output preservation."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile


def checked(command, **kwargs):
    result = subprocess.run(command, capture_output=True, text=True, timeout=90, **kwargs)
    if result.returncode:
        raise RuntimeError('%r exit=%d\n%s%s' % (command, result.returncode, result.stdout, result.stderr))
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True)
    parser.add_argument('--compiler-id', required=True)
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--generator', default='')
    parser.add_argument('--platform', default='')
    parser.add_argument('--toolset', default='')
    parser.add_argument('--config', default='Release')
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    root = args.root.resolve()
    source = (root/'src/i18n.c').read_text(encoding='utf-8')
    # Source-format sensitivity for compilers that do not diagnose unreachable return.
    match = re.search(r'(?ms)^const char \*LangTemplate\(.*?^\}\n', source)
    if not match or re.search(r'\n    return "";\n\}$', match[0].rstrip('\n')):
        raise RuntimeError('LangTemplate terminal unreachable return contract')
    gold = json.loads((root/'tests/i18n_template_gold.json').read_text(encoding='utf-8'))
    lines = ['#include <string.h>', '#include <limits.h>', '#include "i18n.h"', 'int main(void) {',
             'int k, l, v, n; const char *a;']
    for index, row in enumerate(gold['cases'], 1):
        case = dict(zip(gold['case_columns'], row))
        v = 'INT_MIN' if case['variant'] == -2147483648 else str(case['variant'])
        lines.append('if(strcmp(LangTemplate((I18N_KEY)%d,(LANG_ID)%d,%s),%s)) return %d;' %
                     (case['key'], case['language'], v, json.dumps(case['text']), index))
    lines += ['for(k=0;k<I18N_COUNT;++k) {', 'n=LangVariantCount((I18N_KEY)k); if(n<=0) return 900;',
              'for(l=0;l<LANG_COUNT;++l) {', 'LangSet((LANG_ID)l);',
              'for(v=0;v<12;++v) {', 'a=LangTemplate((I18N_KEY)k,(LANG_ID)l,v);',
              'if(strcmp(a,LangTemplate((I18N_KEY)k,(LANG_ID)l,v%n))) return 901;',
              'if(strcmp(a,LangString((I18N_KEY)k,v))) return 902;', '}',
              'if(strcmp(LangTemplate((I18N_KEY)k,(LANG_ID)l,-1),LangTemplate((I18N_KEY)k,(LANG_ID)l,0))) return 903;',
              'if(strcmp(LangTemplate((I18N_KEY)k,(LANG_ID)99,0),LangTemplate((I18N_KEY)k,LANG_EN,0))) return 904;',
              '}', '}', 'return 0;', '}']
    with tempfile.TemporaryDirectory(prefix='symbols-i18n-template-') as directory:
        tmp = Path(directory)
        (tmp/'gold.c').write_text('\n'.join(lines)+'\n', encoding='utf-8')
        if args.compiler_id == 'MSVC':
            project = ['cmake_minimum_required(VERSION 3.16)', 'project(i18n_template_probe C)',
                       'add_executable(gold gold.c "'+(root/'src/i18n.c').as_posix()+'")',
                       'target_include_directories(gold PRIVATE "'+(root/'include').as_posix()+'")',
                       'target_compile_options(gold PRIVATE /W4 /WX)',
                       'target_compile_definitions(gold PRIVATE strcasecmp=_stricmp)']
            (tmp/'CMakeLists.txt').write_text('\n'.join(project)+'\n', encoding='utf-8')
            build = tmp/'build'
            configure = [args.cmake, '-S', str(tmp), '-B', str(build)]
            for flag, value in [('-G', args.generator), ('-A', args.platform), ('-T', args.toolset)]:
                if value:
                    configure += [flag, value]
            checked(configure, cwd=tmp)
            checked([args.cmake, '--build', str(build), '--config', args.config, '--target', 'gold'], cwd=tmp)
            executable = build/args.config/'gold.exe'
            if not executable.is_file():
                executable = build/'gold.exe'
        elif args.compiler_id in ('GNU', 'Clang', 'AppleClang'):
            executable = tmp/'gold'
            checked([args.compiler, '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-I'+str(root/'include'),
                     str(tmp/'gold.c'), str(root/'src/i18n.c'), '-o', str(executable)], cwd=tmp)
        else:
            raise RuntimeError('unreviewed compiler '+args.compiler_id)
        checked([str(executable)], cwd=tmp)
    print('i18n full-TU warning regression PASS compiler=%s snapshots=%d' % (args.compiler_id, len(gold['cases'])))


if __name__ == '__main__':
    main()
