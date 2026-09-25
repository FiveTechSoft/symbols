#!/usr/bin/env python3
"""Opt-in, read-only libclang inspector. Never supplies edit permissions."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import sys
from importlib import metadata


def result(reason, **fields):
    print(json.dumps({"status": "unknown" if reason else "complete", "reason": reason, **fields}, sort_keys=True))
    return 2 if reason else 0


def absolute(name, base):
    return str(Path(base, name).resolve())


def inspect(db_path, source, variant):
    db = Path(db_path).resolve()
    src = str(Path(source).resolve())
    if Path(src).suffix.lower() != '.c':
        return result("c_translation_unit_required", source=src)
    try:
        entries = json.loads(db.read_text(encoding="utf-8"))
        if not isinstance(entries, list):
            return result("invalid_db")
    except (OSError, ValueError, UnicodeError):
        return result("missing_or_invalid_db")
    matches = []
    for i, entry in enumerate(entries):
        if not isinstance(entry, dict) or not isinstance(entry.get("file"), str) or not isinstance(entry.get("directory"), str):
            return result("invalid_db_entry")
        directory = absolute(entry["directory"], db.parent)
        if absolute(entry["file"], directory) == src:
            matches.append((i, entry, directory))
    if not matches:
        return result("tu_not_in_db", source=src)
    if variant is None:
        if len(matches) != 1:
            return result("ambiguous_variant", source=src, variants=[x[0] for x in matches])
        selected = matches[0]
    else:
        selected = next((x for x in matches if x[0] == variant), None)
        if selected is None:
            return result("variant_not_found", source=src, variants=[x[0] for x in matches])
    i, entry, directory = selected
    command = entry.get("arguments")
    if not isinstance(command, list) or not command or not all(isinstance(a, str) for a in command):
        try:
            command = shlex.split(entry["command"], posix=os.name != "nt")
        except (KeyError, ValueError, TypeError):
            return result("invalid_command", source=src, variant=i)
    if not command or not Path(src).is_file() or not Path(directory).is_dir():
        return result("missing_source_or_directory", source=src, variant=i)
    # Preserve the compilation database's flags and ordering. Strip only the
    # compiler, output artifact and compile-only switch; never run the command.
    args = []
    j = 1
    while j < len(command):
        a = command[j]
        if a in ("-o", "--output", "/Fo"):
            j += 2
            continue
        if a.startswith("/Fo") or a.startswith("-o") and a != "-o":
            j += 1
            continue
        if a in ("-c", "/c") or absolute(a, directory) == src:
            j += 1
            continue
        args.append(a)
        j += 1
    if any(a.startswith("@") for a in args):
        return result("response_file_unsupported", source=src, variant=i)
    if any(a in ("-include", "-imacros", "-Xclang", "/FI") or a.startswith("/FI") for a in args):
        return result("unsupported_compiler_options", source=src, variant=i)
    try:
        source_before = hashlib.sha256(Path(src).read_bytes()).hexdigest()
        db_before = hashlib.sha256(db.read_bytes()).hexdigest()
    except OSError:
        return result("source_changed_or_missing", source=src, variant=i)
    try:
        if metadata.version("libclang") != "18.1.1":
            return result("libclang_version_mismatch", source=src, variant=i)
        from clang import cindex
        index = cindex.Index.create()
        # libclang needs the compiler command's directory for relative -I.
        oldcwd = os.getcwd()
        try:
            os.chdir(directory)
            tu = index.parse(src, args=args, options=cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)
        finally:
            os.chdir(oldcwd)
    except Exception:
        # A missing native DLL or parser failure is an explicit unknown.
        return result("parser_unavailable_or_failed", source=src, variant=i)
    if tu is None:
        return result("parser_unavailable_or_failed", source=src, variant=i)
    diagnostics = [{"severity": int(d.severity), "message": d.spelling,
                    "file": str(d.location.file) if d.location.file else None,
                    "line": d.location.line} for d in tu.diagnostics]
    if diagnostics:
        # Even a warning can hide a partial binding. This consumer does not
        # assert a complete semantic view from a diagnostic-bearing TU.
        return result("diagnostics", source=src, variant=i, diagnostics=diagnostics)
    def loc(c):
        x = c.location
        return {"file": str(x.file) if x.file else None, "line": x.line, "column": x.column}
    def span(c):
        x = c.extent
        return {"start": {"line": x.start.line, "column": x.start.column},
                "end": {"line": x.end.line, "column": x.end.column}}
    facts, unresolved = [], []
    for c in tu.cursor.walk_preorder():
        if not c.location.file or str(Path(str(c.location.file)).resolve()) != src:
            continue
        kind = c.kind
        if kind in (cindex.CursorKind.FUNCTION_DECL, cindex.CursorKind.VAR_DECL):
            facts.append({"kind": "declaration", "name": c.spelling, "type": c.type.spelling,
                          "definition": c.is_definition(), "location": loc(c), "span": span(c)})
        elif kind in (cindex.CursorKind.DECL_REF_EXPR, cindex.CursorKind.CALL_EXPR):
            ref = c.referenced
            fact = {"kind": "call" if kind == cindex.CursorKind.CALL_EXPR else "reference",
                    "name": c.spelling, "location": loc(c), "span": span(c)}
            if ref is None or not ref.location.file:
                unresolved.append(fact)
            else:
                # A function-pointer call can resolve to a variable, not a
                # unique callee. Do not present that as a direct call edge.
                if kind == cindex.CursorKind.CALL_EXPR and ref.kind != cindex.CursorKind.FUNCTION_DECL:
                    fact["reason"] = "indirect_call"
                    unresolved.append(fact)
                    continue
                fact["target"] = loc(ref)
                fact["target_name"] = ref.spelling
                facts.append(fact)
        elif kind == cindex.CursorKind.MACRO_INSTANTIATION:
            facts.append({"kind": "macro_expansion", "name": c.spelling, "location": loc(c),
                          "span": span(c), "edit_range_safe": False})
    if unresolved:
        return result("unresolved_bindings", source=src, variant=i,
                      diagnostics=diagnostics, unresolved=unresolved)
    try:
        includes = []
        for inc in tu.get_includes():
            path = str(Path(str(inc.include.name)).resolve())
            includes.append({"file": path, "sha256": hashlib.sha256(Path(path).read_bytes()).hexdigest()})
        digest = hashlib.sha256(Path(src).read_bytes()).hexdigest()
        if digest != source_before or hashlib.sha256(db.read_bytes()).hexdigest() != db_before:
            return result("source_or_db_changed_during_inspection", source=src, variant=i)
    except (OSError, AttributeError, TypeError):
        return result("missing_include_or_changed_source", source=src, variant=i)
    return result(None, source=src, variant=i, directory=directory,
                  command_sha256=hashlib.sha256(json.dumps(command).encode()).hexdigest(),
                  db_sha256=db_before, source_sha256=digest,
                  libclang_version="18.1.1 pinned package", includes=includes,
                  diagnostics=diagnostics, facts=facts,
                  limitations=["read_only", "no_edit_authority", "no_cached_edges", "single_tu_only", "source_file_facts_only"])


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--db", required=True, help="compile_commands.json")
    p.add_argument("--source", required=True, help="exact C translation unit")
    p.add_argument("--variant", type=int, help="zero-based compilation database entry index")
    a = p.parse_args()
    return inspect(a.db, a.source, a.variant)


if __name__ == "__main__":
    sys.exit(main())
