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
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--generator', default='')
    parser.add_argument('--platform', default='')
    parser.add_argument('--toolset', default='')
    parser.add_argument('--config', default='Release')
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
            # Visual Studio's build selects CRT/SDK; CTest need not inherit vcvars.
            # Keep both consumer compilation and implementation under /W4 /WX.
            project = [
                'cmake_minimum_required(VERSION 3.16)',
                'project(i18n_warning_probe C)',
                'add_library(header_probe OBJECT header.c)',
                'add_executable(lookup lookup.c "'+(root/'src/i18n.c').as_posix()+'")',
                'foreach(t header_probe lookup)',
                '  target_include_directories(${t} PRIVATE "'+(root/'include').as_posix()+'")',
                '  target_compile_options(${t} PRIVATE /W4 /WX)',
                'endforeach()',
                'target_compile_definitions(lookup PRIVATE strcasecmp=_stricmp)',
            ]
            (tmp/'CMakeLists.txt').write_text('\n'.join(project)+'\n', encoding='utf-8')
            build = tmp/'build'
            configure = [args.cmake, '-S', str(tmp), '-B', str(build)]
            if args.generator:
                configure += ['-G', args.generator]
            if args.platform:
                configure += ['-A', args.platform]
            if args.toolset:
                configure += ['-T', args.toolset]
            checked(configure, cwd=tmp)
            checked([args.cmake, '--build', str(build), '--config', args.config,
                     '--target', 'header_probe', 'lookup'], cwd=tmp)
            executable = build/args.config/'lookup.exe'
            if not executable.is_file():
                executable = build/'lookup.exe'  # single-config generator
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
