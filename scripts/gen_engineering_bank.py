#!/usr/bin/env python3
"""Generate tests/fixtures/engineering_bank (idempotent).

Each task directory:
  task.md    English natural-language prompt
  before/    reproducible initial workspace
  after/     golden end-state (self-test only; never shown to the agent)
  check.py   objective check; exit 0 = pass; CWD = workdir

Regenerate: python scripts/gen_engineering_bank.py
Self-test:  python scripts/test_engineering_bank.py
"""
from __future__ import annotations

import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BANK = ROOT / "tests" / "fixtures" / "engineering_bank"

CHECK_COMPILE = r'''#!/usr/bin/env python3
import subprocess, sys, glob
from pathlib import Path
files = sorted(glob.glob("*.c"))
if not files:
    sys.exit(2)
cmd = ["gcc", "-std=c11", "-Werror=implicit-function-declaration", "-fsyntax-only"] + files
r = subprocess.run(cmd, capture_output=True, text=True)
if r.returncode != 0:
    sys.stderr.write(r.stderr)
sys.exit(r.returncode)
'''

CHECK_COMPILE_TWO = r'''#!/usr/bin/env python3
import subprocess, sys
from pathlib import Path
need = ["main.c", "util.c"]
for n in need:
    if not Path(n).is_file():
        sys.stderr.write("missing " + n + "\n")
        sys.exit(1)
r = subprocess.run(
    ["gcc", "-std=c11", "-Werror=implicit-function-declaration", "-fsyntax-only"] + need,
    capture_output=True, text=True)
if r.returncode != 0:
    sys.stderr.write(r.stderr)
sys.exit(r.returncode)
'''

# Compile every *.c in CWD and run the binary; exit 0 only if the program exits 0.
# Used where before/ deterministically exits non-zero and after/ exits 0.
CHECK_RUN = r'''#!/usr/bin/env python3
import subprocess, sys, glob
from pathlib import Path
files = sorted(glob.glob("*.c"))
if not files:
    sys.exit(2)
r = subprocess.run(
    ["gcc", "-std=c11", "-Werror=implicit-function-declaration", "-o", "eb_bin"] + files,
    capture_output=True, text=True)
if r.returncode != 0:
    sys.stderr.write(r.stderr)
    sys.exit(1)
bin_path = Path("eb_bin.exe") if Path("eb_bin.exe").is_file() else Path("eb_bin")
if not bin_path.is_file():
    sys.exit(1)
r2 = subprocess.run([str(bin_path.resolve())], capture_output=True, text=True)
sys.exit(r2.returncode)
'''



def write_task(tid: str, category: str, prompt: str,
               before: dict[str, str], after: dict[str, str],
               check: str) -> None:
    d = BANK / category / tid
    if d.exists():
        shutil.rmtree(d)
    (d / "before").mkdir(parents=True)
    (d / "after").mkdir(parents=True)
    (d / "task.md").write_text(prompt.rstrip() + "\n", encoding="utf-8")
    (d / "check.py").write_text(check, encoding="utf-8")
    for rel, content in before.items():
        p = d / "before" / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(content, encoding="utf-8")
    for rel, content in after.items():
        p = d / "after" / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(content, encoding="utf-8")


def contains_check(*must: str, forbid: tuple[str, ...] = ()) -> str:
    if isinstance(forbid, str):  # tolerate missing trailing comma
        forbid = (forbid,)
    parts = [
        "#!/usr/bin/env python3",
        "import re",
        "import sys",
        "from pathlib import Path",
        "blob = ''",
        "skip_names = {'check.py'}",
        "skip = {'.pyc', '.o', '.obj', '.exe', '.bin', '.png', '.jpg'}",
        "for p in Path('.').rglob('*'):",
        "    if not p.is_file() or p.suffix in skip or p.name in skip_names:",
        "        continue",
        "    if p.stat().st_size > 65536:",
        "        continue",
        "    try:",
        "        blob += p.read_text(encoding='utf-8', errors='replace') + '\\n'",
        "    except OSError:",
        "        pass",
        # Normalize whitespace so indentation / blank lines do not flip the check.
        "norm = re.sub(r'\\s+', ' ', blob)",
        "ok = True",
    ]
    for m in must:
        parts.append(f"need = {m!r}")
        parts.append("need_n = re.sub(r'\\s+', ' ', need)")
        parts.append("if need not in blob and need_n not in norm:")
        parts.append("    print('missing', need)")
        parts.append("    ok = False")
    for f in forbid:
        parts.append(f"bad = {f!r}")
        parts.append("bad_n = re.sub(r'\\s+', ' ', bad)")
        parts.append("if bad in blob or bad_n in norm:")
        parts.append("    print('forbidden', bad)")
        parts.append("    ok = False")
    parts.append("sys.exit(0 if ok else 1)")
    return "\n".join(parts) + "\n"


def files_check(required: tuple[str, ...], forbid: tuple[str, ...] = ()) -> str:
    parts = [
        "#!/usr/bin/env python3",
        "import sys",
        "from pathlib import Path",
        "ok = True",
    ]
    for name in required:
        parts.append(f"if not Path({name!r}).is_file():")
        parts.append(f"    print('missing file', {name!r})")
        parts.append("    ok = False")
    for name in forbid:
        parts.append(f"if Path({name!r}).exists():")
        parts.append(f"    print('forbidden file', {name!r})")
        parts.append("    ok = False")
    parts.append("sys.exit(0 if ok else 1)")
    return "\n".join(parts) + "\n"


def _content_snippet(
    must: dict[str, tuple[str, ...]],
    forbid: tuple[tuple[str, str], ...] = (),
    regex: bool = False,
) -> str:
    lines = ["import re", "ok = True"]
    for name, needles in must.items():
        lines.append(f"_t = Path({name!r}).read_text(encoding='utf-8', errors='replace') if Path({name!r}).is_file() else ''")
        for n in needles:
            if regex:
                lines.append(f"if not re.search(r'{n}', _t):")
            else:
                lines.append(f"if {n!r} not in _t:")
            lines.append(f"    print('missing', {n!r}, 'in', {name!r})")
            lines.append("    ok = False")
    for name, bad in forbid:
        lines.append(f"_t = Path({name!r}).read_text(encoding='utf-8', errors='replace') if Path({name!r}).is_file() else ''")
        if regex:
            lines.append(f"if re.search(r'{bad}', _t):")
        else:
            lines.append(f"if {bad!r} in _t:")
        lines.append(f"    print('forbidden', {bad!r}, 'in', {name!r})")
        lines.append("    ok = False")
    lines.append("sys.exit(0 if ok else 1)")
    return "\n".join(lines) + "\n"


def run_and_contains(
    must: dict[str, tuple[str, ...]],
    forbid: tuple[tuple[str, str], ...] = (),
    regex: bool = False,
) -> str:
    """Compile+run all *.c (exit 0 required), then per-file content checks."""
    content = _content_snippet(must, forbid, regex=regex)
    return CHECK_RUN.replace(
        "r2 = subprocess.run([str(bin_path.resolve())], capture_output=True, text=True)\n"
        "sys.exit(r2.returncode)\n",
        "r2 = subprocess.run([str(bin_path.resolve())], capture_output=True, text=True)\n"
        "if r2.returncode != 0:\n"
        "    sys.exit(r2.returncode)\n"
        + content,
    )


def compile_and_contains(
    must: dict[str, tuple[str, ...]],
    forbid: tuple[tuple[str, str], ...] = (),
    regex: bool = False,
) -> str:
    """-fsyntax-only compile of all *.c (must succeed), then content checks."""
    content = _content_snippet(must, forbid, regex=regex)
    return CHECK_COMPILE.replace(
        "if r.returncode != 0:\n"
        "    sys.stderr.write(r.stderr)\n"
        "sys.exit(r.returncode)\n",
        "if r.returncode != 0:\n"
        "    sys.stderr.write(r.stderr)\n"
        "    sys.exit(r.returncode)\n"
        + content,
    )


def run_and_files(required: tuple[str, ...], forbid: tuple[str, ...] = ()) -> str:
    """Compile+run all *.c (exit 0 required), then require/forbid files."""
    file_lines = ["ok = True"]
    for name in required:
        file_lines.append(f"if not Path({name!r}).is_file():")
        file_lines.append(f"    print('missing file', {name!r})")
        file_lines.append("    ok = False")
    for name in forbid:
        file_lines.append(f"if Path({name!r}).exists():")
        file_lines.append(f"    print('forbidden file', {name!r})")
        file_lines.append("    ok = False")
    file_lines.append("sys.exit(0 if ok else 1)")
    content = "\n".join(file_lines) + "\n"
    return CHECK_RUN.replace(
        "r2 = subprocess.run([str(bin_path.resolve())], capture_output=True, text=True)\n"
        "sys.exit(r2.returncode)\n",
        "r2 = subprocess.run([str(bin_path.resolve())], capture_output=True, text=True)\n"
        "if r2.returncode != 0:\n"
        "    sys.exit(r2.returncode)\n"
        + content,
    )


# Runtime shell resolver embedded in every generated shell check.
# Resolved on the host that runs check.py — never baked at generation time.
SH_RESOLVE = r'''import shutil
def _sh_bin():
    for name in ("bash", "sh"):
        path = shutil.which(name)
        if path and "system32" not in path.lower():
            return [path]
    for cand in (
        r"C:\Program Files\Git\bin\bash.exe",
        r"C:\Program Files\Git\usr\bin\sh.exe",
        r"C:\Program Files (x86)\Git\bin\bash.exe",
    ):
        if Path(cand).is_file():
            return [cand]
    path = shutil.which("sh")
    if path:
        return [path]
    return ["sh"]
shell = _sh_bin()
'''


def run_script_check(script: str, expect_out: str) -> str:
    return f'''#!/usr/bin/env python3
import subprocess, sys
from pathlib import Path
{SH_RESOLVE}
if not Path({script!r}).is_file():
    print("missing", {script!r})
    sys.exit(1)
r = subprocess.run(shell + [{script!r}], capture_output=True, text=True)
out = (r.stdout or "") + (r.stderr or "")
if r.returncode != 0:
    print(out)
    sys.exit(1)
if {expect_out!r} and {expect_out!r} not in out:
    print("expected output containing", {expect_out!r}, "got:", out)
    sys.exit(1)
sys.exit(0)
'''


def sh_run_check(argv_tail: str, expect_rc_expr: str, extra: str = "") -> str:
    """Build a check that runs a shell command via a portable shell."""
    return f'''#!/usr/bin/env python3
import subprocess, sys
from pathlib import Path
{SH_RESOLVE}
{extra}
r = subprocess.run(shell + {argv_tail}, capture_output=True, text=True)
rc = r.returncode
ok = ({expect_rc_expr})
if not ok:
    print("rc", rc, "out", (r.stdout or "") + (r.stderr or ""))
sys.exit(0 if ok else 1)
'''


def run_python_check(script: str, expect_out: str) -> str:
    return f'''#!/usr/bin/env python3
import subprocess, sys
from pathlib import Path
if not Path({script!r}).is_file():
    print("missing", {script!r})
    sys.exit(1)
r = subprocess.run([sys.executable, {script!r}], capture_output=True, text=True)
out = (r.stdout or "") + (r.stderr or "")
if r.returncode != 0:
    print(out)
    sys.exit(1)
if {expect_out!r} not in out:
    print("expected", {expect_out!r}, "got:", out)
    sys.exit(1)
sys.exit(0)
'''


def main() -> int:
    if BANK.exists():
        shutil.rmtree(BANK)
    BANK.mkdir(parents=True)

    tasks: list[tuple[str, str]] = []

    # ---- compiler_repair (7) -------------------------------------------------
    cr = [
        ("eb_cr_001",
         "The program fails to compile because printf is undeclared. Add the correct standard header so it builds with -std=c11 -Werror=implicit-function-declaration. Do not change the logic.",
         {"main.c": "int main(void) {\n    printf(\"ok\\n\");\n    return 0;\n}\n"},
         {"main.c": "#include <stdio.h>\n\nint main(void) {\n    printf(\"ok\\n\");\n    return 0;\n}\n"},
         CHECK_COMPILE),
        ("eb_cr_002",
         "A typo breaks the build: the variable is declared total_count but referenced as total_cont. Fix the misspelling so the file compiles. Keep the same behavior.",
         {"main.c": "int main(void) {\n    int total_count = 1;\n    return total_cont;\n}\n"},
         {"main.c": "int main(void) {\n    int total_count = 1;\n    return total_count;\n}\n"},
         CHECK_COMPILE),
        ("eb_cr_003",
         "helper_sum is used before it is declared, which fails with -Werror=implicit-function-declaration. Provide a prototype (or definition before use) so the file compiles.",
         {"main.c": "int main(void) {\n    return helper_sum(1, 2);\n}\n\nint helper_sum(int a, int b) {\n    return a + b;\n}\n"},
         {"main.c": "int helper_sum(int a, int b);\n\nint main(void) {\n    return helper_sum(1, 2);\n}\n\nint helper_sum(int a, int b) {\n    return a + b;\n}\n"},
         CHECK_COMPILE),
        ("eb_cr_004",
         "There is a missing semicolon after the return statement. Fix the syntax error so the file compiles.",
         {"main.c": "int main(void) {\n    return 0\n}\n"},
         {"main.c": "int main(void) {\n    return 0;\n}\n"},
         CHECK_COMPILE),
        ("eb_cr_005",
         "The identifier buffer_size is used but never declared. Introduce a local variable (or parameter) with that name so the file compiles; keep returning 0 from main.",
         {"main.c": "int main(void) {\n    return buffer_size > 0 ? 0 : 0;\n}\n"},
         {"main.c": "int main(void) {\n    int buffer_size = 1;\n    return buffer_size > 0 ? 0 : 0;\n}\n"},
         CHECK_COMPILE),
        ("eb_cr_006",
         "strlen is used without including <string.h>. Add the right header so the file compiles.",
         {"main.c": "#include <stddef.h>\n\nint main(void) {\n    const char *s = \"abc\";\n    return (int)strlen(s) - 3;\n}\n"},
         {"main.c": "#include <stddef.h>\n#include <string.h>\n\nint main(void) {\n    const char *s = \"abc\";\n    return (int)strlen(s) - 3;\n}\n"},
         CHECK_COMPILE),
        ("eb_cr_007",
         "There is an extra closing brace after main, which is a syntax error. Remove it so the file compiles as a single translation unit.",
         {"main.c": "int main(void) {\n    return 0;\n}\n}\n"},
         {"main.c": "int main(void) {\n    return 0;\n}\n"},
         CHECK_COMPILE),
    ]
    for tid, prompt, b, a, c in cr:
        write_task(tid, "compiler_repair", prompt, b, a, c)
        tasks.append((tid, "compiler_repair"))

    # ---- refactor (7) --------------------------------------------------------
    rf = [
        ("eb_rf_001",
         "Replace the magic number 42 with a named constant MAX_ITEMS. The program must still exit 0. Forbidden: the bare token 42 must not appear outside the constant definition line.",
         {"main.c": "int main(void) {\n    int n = 42;\n    return n == 42 ? 0 : 1;\n}\n"},
         {"main.c": "#define MAX_ITEMS 42\n\nint main(void) {\n    int n = MAX_ITEMS;\n    return n == MAX_ITEMS ? 0 : 1;\n}\n"},
         run_and_contains(
             {"main.c": ("MAX_ITEMS",)},
             forbid=(("main.c", "int n = 42;"),))),
        ("eb_rf_002",
         "Rename the function compute to compute_total everywhere in main.c. Do not leave the old name. The program must exit 0.",
         {"main.c": "static int compute(int a) { return a + 1; }\n\nint main(void) {\n    return compute(1) == 2 ? 0 : 1;\n}\n"},
         {"main.c": "static int compute_total(int a) { return a + 1; }\n\nint main(void) {\n    return compute_total(1) == 2 ? 0 : 1;\n}\n"},
         run_and_contains(
             {"main.c": ("compute_total",)},
             forbid=(("main.c", "compute("), ("main.c", "static int compute(")))),
        ("eb_rf_003",
         "Remove the unused function dead_helper so only live code remains. main must stay and the program must exit 0.",
         {"main.c": "static int dead_helper(int x) { return x; }\n\nint main(void) {\n    return 0;\n}\n"},
         {"main.c": "int main(void) {\n    return 0;\n}\n"},
         run_and_contains(
             {"main.c": ("int main",)},
             forbid=(("main.c", "dead_helper"),))),
        ("eb_rf_004",
         "Make the pointer parameter of scale const-correct: the function must not modify *p, and the signature must use const int *. The program must exit 0.",
         {"main.c": "static int scale(int *p, int k) {\n    return *p * k;\n}\n\nint main(void) {\n    int v = 2;\n    return scale(&v, 3) == 6 ? 0 : 1;\n}\n"},
         {"main.c": "static int scale(const int *p, int k) {\n    return *p * k;\n}\n\nint main(void) {\n    int v = 2;\n    return scale(&v, 3) == 6 ? 0 : 1;\n}\n"},
         run_and_contains(
             {"main.c": ("const int *",)},
             forbid=(("main.c", "static int scale(int *p"),))),
        ("eb_rf_005",
         "Extract the repeated literal \"RESULT: \" into a named constant RESULT_PREFIX and use it in both printf calls. The program must exit 0.",
         {"main.c": "#include <stdio.h>\n\nint main(void) {\n    printf(\"RESULT: %d\\n\", 1);\n    printf(\"RESULT: %d\\n\", 2);\n    return 0;\n}\n"},
         {"main.c": "#include <stdio.h>\n\n#define RESULT_PREFIX \"RESULT: \"\n\nint main(void) {\n    printf(RESULT_PREFIX \"%d\\n\", 1);\n    printf(RESULT_PREFIX \"%d\\n\", 2);\n    return 0;\n}\n"},
         run_and_contains(
             {"main.c": ("RESULT_PREFIX",)},
             forbid=(("main.c", 'printf("RESULT: %d'),))),
        ("eb_rf_006",
         "Convert the while loop that counts to 3 into a for loop with the same semantics. The program must exit 0 and the file must contain a for loop, not while.",
         {"main.c": "int main(void) {\n    int i = 0;\n    while (i < 3) {\n        i++;\n    }\n    return i == 3 ? 0 : 1;\n}\n"},
         {"main.c": "int main(void) {\n    int i;\n    for (i = 0; i < 3; i++) {\n    }\n    return i == 3 ? 0 : 1;\n}\n"},
         run_and_contains(
             {"main.c": ("for (",)},
             forbid=(("main.c", "while ("),))),
        ("eb_rf_007",
         "Replace the goto-based flow with structured control flow (no goto). Keep returning 0 on the success path; the program must exit 0 and must not contain goto.",
         {"main.c": "int main(void) {\n    int ok = 1;\n    if (!ok) goto fail;\n    return 0;\nfail:\n    return 1;\n}\n"},
         {"main.c": "int main(void) {\n    int ok = 1;\n    if (!ok) {\n        return 1;\n    }\n    return 0;\n}\n"},
         run_and_contains(
             {"main.c": ("int main",)},
             forbid=(("main.c", "goto"),))),
    ]
    for tid, prompt, b, a, c in rf:
        write_task(tid, "refactor", prompt, b, a, c)
        tasks.append((tid, "refactor"))

    # ---- test_authoring (7) --------------------------------------------------
    # Shared pattern: broken/missing test; after has a runnable self-check.
    ta = [
        ("eb_ta_001",
         "Add a test that checks square(3) equals 9. Provide test_foo.c with a main that returns 0 on success and non-zero on failure. It must compile and run successfully.",
         {"util.c": "int square(int x) { return x * x; }\n",
          "util.h": "int square(int x);\n"},
         {"util.c": "int square(int x) { return x * x; }\n",
          "util.h": "int square(int x);\n",
          "test_foo.c": "#include \"util.h\"\n\nint main(void) {\n    return square(3) == 9 ? 0 : 1;\n}\n"},
         '''#!/usr/bin/env python3
import subprocess, sys
from pathlib import Path
if not Path("test_foo.c").is_file():
    sys.exit(1)
r = subprocess.run(["gcc", "-std=c11", "-Werror=implicit-function-declaration",
                    "-o", "test_foo_bin", "test_foo.c", "util.c"], capture_output=True, text=True)
if r.returncode != 0:
    sys.stderr.write(r.stderr); sys.exit(1)
r = subprocess.run(["./test_foo_bin"], capture_output=True)
sys.exit(r.returncode)
'''),
        ("eb_ta_002",
         "The existing test_foo.c asserts the wrong expected value (square(3) should be 9, not 8). Fix the assertion so the test passes.",
         {"util.c": "int square(int x) { return x * x; }\n",
          "util.h": "int square(int x);\n",
          "test_foo.c": "#include \"util.h\"\n\nint main(void) {\n    return square(3) == 8 ? 0 : 1;\n}\n"},
         {"util.c": "int square(x) { return x * x; }\n".replace("int square(x)", "int square(int x)"),
          "util.h": "int square(int x);\n",
          "test_foo.c": "#include \"util.h\"\n\nint main(void) {\n    return square(3) == 9 ? 0 : 1;\n}\n"},
         '''#!/usr/bin/env python3
import subprocess, sys
from pathlib import Path
r = subprocess.run(["gcc", "-std=c11", "-Werror=implicit-function-declaration",
                    "-o", "test_foo_bin", "test_foo.c", "util.c"], capture_output=True, text=True)
if r.returncode != 0:
    sys.stderr.write(r.stderr); sys.exit(1)
r = subprocess.run(["./test_foo_bin"], capture_output=True)
sys.exit(r.returncode)
'''),
        ("eb_ta_003",
         "Add an edge-case test for clamp(0, 1, 10) expecting 1. Put it in test_foo.c; the binary must exit 0.",
         {"util.c": "int clamp(int v, int lo, int hi) {\n    if (v < lo) return lo;\n    if (v > hi) return hi;\n    return v;\n}\n",
          "util.h": "int clamp(int v, int lo, int hi);\n"},
         {"util.c": "int clamp(int v, int lo, int hi) {\n    if (v < lo) return lo;\n    if (v > hi) return hi;\n    return v;\n}\n",
          "util.h": "int clamp(int v, int lo, int hi);\n",
          "test_foo.c": "#include \"util.h\"\n\nint main(void) {\n    return clamp(0, 1, 10) == 1 ? 0 : 1;\n}\n"},
         '''#!/usr/bin/env python3
import subprocess, sys
from pathlib import Path
if not Path("test_foo.c").is_file():
    sys.exit(1)
r = subprocess.run(["gcc", "-std=c11", "-Werror=implicit-function-declaration",
                    "-o", "test_foo_bin", "test_foo.c", "util.c"], capture_output=True, text=True)
if r.returncode != 0:
    sys.stderr.write(r.stderr); sys.exit(1)
r = subprocess.run(["./test_foo_bin"], capture_output=True)
sys.exit(r.returncode)
'''),
        ("eb_ta_004",
         "test_foo.c does not compile (missing include of util.h). Fix the test file so it compiles and passes.",
         {"util.c": "int add(int a, int b) { return a + b; }\n",
          "util.h": "int add(int a, int b);\n",
          "test_foo.c": "int main(void) {\n    return add(2, 3) == 5 ? 0 : 1;\n}\n"},
         {"util.c": "int add(int a, int b) { return a + b; }\n",
          "util.h": "int add(int a, int b);\n",
          "test_foo.c": "#include \"util.h\"\n\nint main(void) {\n    return add(2, 3) == 5 ? 0 : 1;\n}\n"},
         '''#!/usr/bin/env python3
import subprocess, sys
r = subprocess.run(["gcc", "-std=c11", "-Werror=implicit-function-declaration",
                    "-o", "test_foo_bin", "test_foo.c", "util.c"], capture_output=True, text=True)
if r.returncode != 0:
    sys.stderr.write(r.stderr); sys.exit(1)
r = subprocess.run(["./test_foo_bin"], capture_output=True)
sys.exit(r.returncode)
'''),
        ("eb_ta_005",
         "Write a negative test: is_empty on a non-empty string must return 0. Create test_foo.c that exits 0 when the assertion holds.",
         {"util.c": "#include <stddef.h>\n\nint is_empty(const char *s) {\n    return s == NULL || s[0] == '\\0';\n}\n",
          "util.h": "int is_empty(const char *s);\n"},
         {"util.c": "#include <stddef.h>\n\nint is_empty(const char *s) {\n    return s == NULL || s[0] == '\\0';\n}\n",
          "util.h": "int is_empty(const char *s);\n",
          "test_foo.c": "#include \"util.h\"\n\nint main(void) {\n    return is_empty(\"x\") == 0 ? 0 : 1;\n}\n"},
         '''#!/usr/bin/env python3
import subprocess, sys
from pathlib import Path
if not Path("test_foo.c").is_file():
    sys.exit(1)
r = subprocess.run(["gcc", "-std=c11", "-Werror=implicit-function-declaration",
                    "-o", "test_foo_bin", "test_foo.c", "util.c"], capture_output=True, text=True)
if r.returncode != 0:
    sys.stderr.write(r.stderr); sys.exit(1)
r = subprocess.run(["./test_foo_bin"], capture_output=True)
sys.exit(r.returncode)
'''),
        ("eb_ta_006",
         "The test runner script run.sh should compile and run test_foo.c and exit with the test status. It currently exits 1 always. Fix run.sh; the checker runs run.sh and expects exit 0.",
         {"util.c": "int double_it(int x) { return x * 2; }\n",
          "util.h": "int double_it(int x);\n",
          "test_foo.c": "#include \"util.h\"\n\nint main(void) {\n    return double_it(4) == 8 ? 0 : 1;\n}\n",
          "run.sh": "#!/bin/sh\nexit 1\n"},
         {"util.c": "int double_it(int x) { return x * 2; }\n",
          "util.h": "int double_it(int x);\n",
          "test_foo.c": "#include \"util.h\"\n\nint main(void) {\n    return double_it(4) == 8 ? 0 : 1;\n}\n",
          "run.sh": "#!/bin/sh\nset -e\ngcc -std=c11 -Werror=implicit-function-declaration -o test_foo_bin test_foo.c util.c\n./test_foo_bin\n"},
         run_script_check("run.sh", "")),
        ("eb_ta_007",
         "Add a test for identity(x) that asserts identity(7) == 7 in test_foo.c. Binary must exit 0.",
         {"util.c": "int identity(int x) { return x; }\n",
          "util.h": "int identity(int x);\n"},
         {"util.c": "int identity(int x) { return x; }\n",
          "util.h": "int identity(int x);\n",
          "test_foo.c": "#include \"util.h\"\n\nint main(void) {\n    return identity(7) == 7 ? 0 : 1;\n}\n"},
         '''#!/usr/bin/env python3
import subprocess, sys
from pathlib import Path
if not Path("test_foo.c").is_file():
    sys.exit(1)
r = subprocess.run(["gcc", "-std=c11", "-Werror=implicit-function-declaration",
                    "-o", "test_foo_bin", "test_foo.c", "util.c"], capture_output=True, text=True)
if r.returncode != 0:
    sys.stderr.write(r.stderr); sys.exit(1)
r = subprocess.run(["./test_foo_bin"], capture_output=True)
sys.exit(r.returncode)
'''),
    ]
    # Fix eb_ta_002 after dict (typo guard)
    ta[1] = (
        ta[1][0],
        ta[1][1],
        ta[1][2],
        {"util.c": "int square(int x) { return x * x; }\n",
         "util.h": "int square(int x);\n",
         "test_foo.c": "#include \"util.h\"\n\nint main(void) {\n    return square(3) == 9 ? 0 : 1;\n}\n"},
        ta[1][4],
    )
    for tid, prompt, b, a, c in ta:
        write_task(tid, "test_authoring", prompt, b, a, c)
        tasks.append((tid, "test_authoring"))

    # ---- build_ci (7) --------------------------------------------------------
    bi = [
        ("eb_bi_001",
         "CMakeLists.txt does not compile main.c (the add_executable list is empty). Add main.c so configure+build would include it. Checker only verifies CMakeLists.txt lists main.c and that main.c still exists.",
         {"CMakeLists.txt": "cmake_minimum_required(VERSION 3.10)\nproject(demo C)\nadd_executable(demo)\n",
          "main.c": "int main(void) { return 0; }\n"},
         {"CMakeLists.txt": "cmake_minimum_required(VERSION 3.10)\nproject(demo C)\nadd_executable(demo main.c)\n",
          "main.c": "int main(void) { return 0; }\n"},
         contains_check("add_executable(demo main.c)", forbid=("add_executable(demo)\n",))),
        ("eb_bi_002",
         "The CI shell script build.sh does not invoke a C compiler. Make it run gcc -std=c11 -fsyntax-only on main.c and exit with gcc's status.",
         {"build.sh": "#!/bin/sh\necho building\nexit 0\n",
          "main.c": "int main(void) { return 0; }\n"},
          {"build.sh": "#!/bin/sh\ngcc -std=c11 -fsyntax-only main.c\n",
           "main.c": "int main(void) { return 0; }\n"},
          contains_check("gcc", forbid=("echo building",))),
        ("eb_bi_003",
         "Makefile target all does not depend on app. Fix the Makefile so `make -n all` would build app (checker: Makefile contains 'all: app').",
         {"Makefile": "all:\n\t@echo done\n",
          "app.c": "int main(void) { return 0; }\n"},
         {"Makefile": "all: app\n\napp: app.c\n\tgcc -std=c11 -o app app.c\n",
          "app.c": "int main(void) { return 0; }\n"},
         contains_check("all: app")),
        ("eb_bi_004",
         "CMakeLists.txt is missing project(). Add a project(<name> C) line after cmake_minimum_required.",
         {"CMakeLists.txt": "cmake_minimum_required(VERSION 3.10)\n"},
         {"CMakeLists.txt": "cmake_minimum_required(VERSION 3.10)\nproject(demo C)\n"},
         contains_check("project(")),
        ("eb_bi_005",
         "build.sh ignores compiler failures (always exits 0). Propagate the compiler exit status (set -e or explicit check). The checker builds a failing main.c case: actually the workdir main.c is valid; checker only requires build.sh not to hardcode exit 0 after a failed gcc. It runs build.sh on the provided main.c which succeeds; additionally greps that the script does not end with a blind 'exit 0' after echo-only build.",
         {"build.sh": "#!/bin/sh\necho compile\nexit 0\n",
          "main.c": "int main(void) { return 0; }\n"},
         {"build.sh": "#!/bin/sh\nset -e\ngcc -std=c11 -fsyntax-only main.c\n",
          "main.c": "int main(void) { return 0; }\n"},
         contains_check("gcc", forbid=("exit 0",))),
        ("eb_bi_006",
         "CMakeLists.txt registers an executable but the source file main.c is missing from disk. Recreate a minimal main.c (int main(void) { return 0; }) so the declared source exists.",
         {"CMakeLists.txt": "cmake_minimum_required(VERSION 3.10)\nproject(demo C)\nadd_executable(demo main.c)\n"},
         {"CMakeLists.txt": "cmake_minimum_required(VERSION 3.10)\nproject(demo C)\nadd_executable(demo main.c)\n",
          "main.c": "int main(void) { return 0; }\n"},
         files_check(("main.c", "CMakeLists.txt"))),
        ("eb_bi_007",
         "The GitHub Actions workflow file ci.yml must contain a step that runs ctest. Add it under jobs.build.steps as a run: ctest line (any indentation). Checker only requires the file to exist and contain 'ctest'.",
         {"ci.yml": "name: ci\non: [push]\njobs:\n  build:\n    runs-on: ubuntu-latest\n    steps:\n      - uses: actions/checkout@v4\n"},
         {"ci.yml": "name: ci\non: [push]\njobs:\n  build:\n    runs-on: ubuntu-latest\n    steps:\n      - uses: actions/checkout@v4\n      - run: ctest --test-dir build\n"},
         contains_check("ctest")),
    ]
    for tid, prompt, b, a, c in bi:
        write_task(tid, "build_ci", prompt, b, a, c)
        tasks.append((tid, "build_ci"))

    # ---- docs (7) ------------------------------------------------------------
    dc = [
        ("eb_dc_001",
         "README.md documents the function as compute_total but the code exports compute. Fix the README so it matches the actual symbol name 'compute'.",
         {"main.c": "int compute(int a) { return a; }\n",
          "README.md": "# Demo\n\nCall `compute_total(x)` to compute a value.\n"},
         {"main.c": "int compute(int a) { return a; }\n",
          "README.md": "# Demo\n\nCall `compute(x)` to compute a value.\n"},
         contains_check("compute", forbid=("compute_total",))),
        ("eb_dc_002",
         "The code block in README.md is fenced incorrectly (missing closing ```). Fix the markdown so the fence is balanced (even number of ``` lines).",
         {"README.md": "# Usage\n\n```c\nint main(void) { return 0; }\n"},
         {"README.md": "# Usage\n\n```c\nint main(void) { return 0; }\n```\n"},
         '''#!/usr/bin/env python3
import sys
from pathlib import Path
t = Path("README.md").read_text(encoding="utf-8")
sys.exit(0 if t.count("```") % 2 == 0 and t.count("```") >= 2 else 1)
'''),
        ("eb_dc_003",
         "README example calls helper() but the source defines helper(int). Update the README signature to helper(int n).",
         {"util.c": "int helper(int n) { return n; }\n",
          "README.md": "# API\n\n`helper()` returns its argument.\n"},
         {"util.c": "int helper(int n) { return n; }\n",
          "README.md": "# API\n\n`helper(int n)` returns its argument.\n"},
         contains_check("helper(int n)", forbid=("helper()",))),
        ("eb_dc_004",
         "Docs say the tool is installed with 'pip install demo' but the project is CMake. Replace that line with 'cmake -S . -B build'. Forbidden: 'pip install'.",
         {"README.md": "# Install\n\npip install demo\n"},
         {"README.md": "# Install\n\ncmake -S . -B build\n"},
         contains_check("cmake -S . -B build", forbid=("pip install",))),
        ("eb_dc_005",
         "The function comment lies: it says returns -1 on error but the code returns 0 on success only. Update the comment to say 'returns 0 on success'. The program must still exit 0.",
         {"main.c": "/* returns -1 on error */\nint parse_ok(void) { return 0; }\n\nint main(void) { return parse_ok(); }\n"},
         {"main.c": "/* returns 0 on success */\nint parse_ok(void) { return 0; }\n\nint main(void) { return parse_ok(); }\n"},
         run_and_contains(
             {"main.c": ("returns 0 on success",)},
             forbid=(("main.c", "returns -1 on error"),))),
        ("eb_dc_006",
         "Add a '## License' section to README.md with the text 'MIT'. The file currently has no license section.",
         {"README.md": "# Project\n\nHello.\n"},
         {"README.md": "# Project\n\nHello.\n\n## License\n\nMIT\n"},
         contains_check("## License", "MIT")),
        ("eb_dc_007",
         "The documented flag --verbose does not match the code which checks for --debug. Update README to document --debug only. Forbidden: --verbose.",
         {"main.c": "int main(int argc, char **argv) {\n    for (int i = 1; i < argc; i++)\n        if (argv[i][0] == '-' && argv[i][1] == '-' ) { /* --debug */ }\n    return 0;\n}\n",
          "README.md": "# Flags\n\nUse --verbose for logs.\n"},
         {"main.c": "int main(int argc, char **argv) {\n    for (int i = 1; i < argc; i++)\n        if (argv[i][0] == '-' && argv[i][1] == '-' ) { /* --debug */ }\n    return 0;\n}\n",
          "README.md": "# Flags\n\nUse --debug for logs.\n"},
         contains_check("--debug", forbid=("--verbose",))),
    ]
    for tid, prompt, b, a, c in dc:
        write_task(tid, "docs", prompt, b, a, c)
        tasks.append((tid, "docs"))

    # ---- shell (7) -----------------------------------------------------------
    sh = [
        ("eb_sh_001",
         "run.sh must print exactly HELLO_SHELL (with newline). It currently prints nothing useful. Checker runs `sh run.sh` and requires that line.",
         {"run.sh": "#!/bin/sh\necho nope\n"},
         {"run.sh": "#!/bin/sh\necho HELLO_SHELL\n"},
         run_script_check("run.sh", "HELLO_SHELL")),
        ("eb_sh_002",
         "run.sh should exit with status 3 when the argument is 'fail' and 0 otherwise. Currently always exits 0. Fix the script; the checker invokes the shell with run.sh fail and expects exit 3.",
         {"run.sh": "#!/bin/sh\nexit 0\n"},
         {"run.sh": "#!/bin/sh\nif [ \"$1\" = \"fail\" ]; then\n    exit 3\nfi\nexit 0\n"},
         sh_run_check('["run.sh", "fail"]', "rc == 3")),
        ("eb_sh_003",
         "run.sh uses an unquoted variable so a path with spaces/multiple spaces is collapsed. The checker runs `sh run.sh 'a  b'` (two spaces) and requires the stdout line to be exactly `a  b`.",
         {"run.sh": "#!/bin/sh\necho $1\n"},
         {"run.sh": "#!/bin/sh\necho \"$1\"\n"},
         f'''#!/usr/bin/env python3
import subprocess, sys
from pathlib import Path
{SH_RESOLVE}
r = subprocess.run(shell + ["run.sh", "a  b"], capture_output=True, text=True)
if r.returncode != 0:
    sys.exit(1)
sys.exit(0 if (r.stdout or "") == "a  b\\n" else 1)
'''),
        ("eb_sh_004",
         "run.sh is missing a shebang. Add #!/bin/sh as the first line. Checker requires the file to start with #!/bin/sh.",
         {"run.sh": "echo ok\n"},
         {"run.sh": "#!/bin/sh\necho ok\n"},
         '''#!/usr/bin/env python3
import sys
from pathlib import Path
t = Path("run.sh").read_text(encoding="utf-8")
sys.exit(0 if t.startswith("#!/bin/sh") else 1)
'''),
        ("eb_sh_005",
         "run.sh must fail closed: if any command fails, the script must exit non-zero before later commands run. Currently it continues after a failure. Fix with set -e (or equivalent). The checker runs the script (which contains a failing command) and requires a non-zero exit.",
         {"run.sh": "#!/bin/sh\nfalse\necho hi\n"},
         {"run.sh": "#!/bin/sh\nset -e\nfalse\necho hi\n"},
         f'''#!/usr/bin/env python3
import subprocess, sys
from pathlib import Path
{SH_RESOLVE}
r = subprocess.run(shell + ["run.sh"], capture_output=True, text=True)
sys.exit(0 if r.returncode != 0 else 1)
'''),
        ("eb_sh_006",
         "run.sh must create output.txt with content OK using a redirection. Checker runs the script then requires output.txt to contain OK.",
         {"run.sh": "#!/bin/sh\ntouch output.txt\n"},
         {"run.sh": "#!/bin/sh\necho OK > output.txt\n"},
         f'''#!/usr/bin/env python3
import subprocess, sys
from pathlib import Path
{SH_RESOLVE}
r = subprocess.run(shell + ["run.sh"], capture_output=True, text=True)
if r.returncode != 0:
    print((r.stdout or "") + (r.stderr or ""))
    sys.exit(1)
p = Path("output.txt")
sys.exit(0 if p.is_file() and p.read_text(encoding="utf-8").strip() == "OK" else 1)
'''),
        ("eb_sh_007",
         "run.sh should exit 3 when helper.sh is missing (fail closed). Implement a file-existence guard. Checker runs in a dir without helper.sh and expects exit 3 (a code no shell uses for a missing script: dash exits 2, bash 127).",
         {"run.sh": "#!/bin/sh\nsh helper.sh\n"},
         {"run.sh": "#!/bin/sh\nif [ -f helper.sh ]; then\n    sh helper.sh\nelse\n    exit 3\nfi\n"},
         sh_run_check('["run.sh"]', "rc == 3")),
    ]
    for tid, prompt, b, a, c in sh:
        write_task(tid, "shell", prompt, b, a, c)
        tasks.append((tid, "shell"))

    # ---- multi_file (7) ------------------------------------------------------
    mf = [
        ("eb_mf_001",
         "Rename the shared constant in util.h from OLD_LIMIT to NEW_LIMIT and update util.c to use it. Both files must agree; main.c may call util. Forbidden: OLD_LIMIT anywhere.",
         {"util.h": "#define OLD_LIMIT 10\nint get_limit(void);\n",
          "util.c": "#include \"util.h\"\nint get_limit(void) { return OLD_LIMIT; }\n",
          "main.c": "#include \"util.h\"\nint main(void) { return get_limit() == 10 ? 0 : 1; }\n"},
         {"util.h": "#define NEW_LIMIT 10\nint get_limit(void);\n",
          "util.c": "#include \"util.h\"\nint get_limit(void) { return NEW_LIMIT; }\n",
          "main.c": "#include \"util.h\"\nint main(void) { return get_limit() == 10 ? 0 : 1; }\n"},
         run_and_contains(
             {"util.h": ("NEW_LIMIT",), "util.c": ("NEW_LIMIT",)},
             forbid=(("util.h", "OLD_LIMIT"), ("util.c", "OLD_LIMIT")))),
        ("eb_mf_002",
         "util.c defines helper but util.h does not declare it. main.c must include util.h and must not carry a local prototype; add the prototype to util.h so the program compiles with -Werror=implicit-function-declaration. Parameter names may match the definition (int x) or be omitted (int).",
         {"util.h": "/* helpers */\n",
          "util.c": "int helper(int x) { return x + 1; }\n",
          "main.c": "#include \"util.h\"\nint main(void) { return helper(1) == 2 ? 0 : 1; }\n"},
         {"util.h": "int helper(int x);\n",
          "util.c": "int helper(int x) { return x + 1; }\n",
          "main.c": "#include \"util.h\"\nint main(void) { return helper(1) == 2 ? 0 : 1; }\n"},
         compile_and_contains(
             {"util.h": (r"\bint\s+helper\s*\(\s*int(?:\s+\w+)?\s*\)",)},
             regex=True)),
        ("eb_mf_003",
         "main.c calls add() but has no include and no local prototype; the definition lives only in util.c. Create util.h with the prototype, include it from both util.c and main.c, so the program compiles with -Werror=implicit-function-declaration and exits 0.",
         {"util.c": "int add(int a, int b) { return a + b; }\n",
          "main.c": "int main(void) { return add(1, 2) == 3 ? 0 : 1; }\n"},
         {"util.h": "int add(int a, int b);\n",
          "util.c": "#include \"util.h\"\nint add(int a, int b) { return a + b; }\n",
          "main.c": "#include \"util.h\"\nint main(void) { return add(1, 2) == 3 ? 0 : 1; }\n"},
         run_and_contains(
             {"main.c": ('#include "util.h"',), "util.c": ('#include "util.h"',),
              "util.h": ("int add(int a, int b);",)})),
        ("eb_mf_004",
         "The include guard in util.h is wrong (uses MAIN_H). Change it to UTIL_H in both #ifndef and #define so the program still exits 0 and the guard names match.",
         {"util.h": "#ifndef MAIN_H\n#define MAIN_H\nint util_value(void);\n#endif\n",
          "util.c": "#include \"util.h\"\nint util_value(void) { return 1; }\n",
          "main.c": "#include \"util.h\"\nint main(void) { return util_value() == 1 ? 0 : 1; }\n"},
         {"util.h": "#ifndef UTIL_H\n#define UTIL_H\nint util_value(void);\n#endif\n",
          "util.c": "#include \"util.h\"\nint util_value(void) { return 1; }\n",
          "main.c": "#include \"util.h\"\nint main(void) { return util_value() == 1 ? 0 : 1; }\n"},
         run_and_contains(
             {"util.h": ("#ifndef UTIL_H", "#define UTIL_H")},
             forbid=(("util.h", "MAIN_H"),))),
        ("eb_mf_005",
         "Split process() out of main.c into process.c with a prototype in process.h; main.c must include process.h and call process(). Require process.c and process.h to exist, and the linked program must exit 0.",
         {"main.c": "static int process(int x) { return x + 1; }\n\nint main(void) {\n    return process(1) == 2 ? 0 : 1;\n}\n"},
         {"process.h": "int process(int x);\n",
          "process.c": "#include \"process.h\"\nint process(int x) { return x + 1; }\n",
          "main.c": "#include \"process.h\"\n\nint main(void) {\n    return process(1) == 2 ? 0 : 1;\n}\n"},
         run_and_files(("process.c", "process.h"))),
        ("eb_mf_006",
         "Two headers define the same macro LIMIT differently. Unify them: both a.h and b.h must define LIMIT as 5 (exact token '5'). The program links one check TU per header and must exit 0 only when both are 5.",
         {"a.h": "#define LIMIT 3\n",
          "b.h": "#define LIMIT 4\n",
          "a_check.c": "#include \"a.h\"\nint a_ok(void) { return LIMIT == 5 ? 1 : 0; }\n",
          "b_check.c": "#include \"b.h\"\nint b_ok(void) { return LIMIT == 5 ? 1 : 0; }\n",
          "main.c": "int a_ok(void);\nint b_ok(void);\nint main(void) { return (a_ok() && b_ok()) ? 0 : 1; }\n"},
         {"a.h": "#define LIMIT 5\n",
          "b.h": "#define LIMIT 5\n",
          "a_check.c": "#include \"a.h\"\nint a_ok(void) { return LIMIT == 5 ? 1 : 0; }\n",
          "b_check.c": "#include \"b.h\"\nint b_ok(void) { return LIMIT == 5 ? 1 : 0; }\n",
          "main.c": "int a_ok(void);\nint b_ok(void);\nint main(void) { return (a_ok() && b_ok()) ? 0 : 1; }\n"},
         CHECK_RUN),
        ("eb_mf_007",
         "main.c calls get() but does not include util.h and has no local prototype. Add the include so the program compiles with -Werror=implicit-function-declaration and exits 0.",
         {"util.h": "int get(void);\n",
          "util.c": "#include \"util.h\"\nint get(void) { return 1; }\n",
          "main.c": "int main(void) { return get() == 1 ? 0 : 1; }\n"},
         {"util.h": "int get(void);\n",
          "util.c": "#include \"util.h\"\nint get(void) { return 1; }\n",
          "main.c": "#include \"util.h\"\nint main(void) { return get() == 1 ? 0 : 1; }\n"},
         run_and_contains({"main.c": ('#include "util.h"',)})),
    ]
    for tid, prompt, b, a, c in mf:
        write_task(tid, "multi_file", prompt, b, a, c)
        tasks.append((tid, "multi_file"))

    # ---- debug (7) -----------------------------------------------------------
    dbg = [
        ("eb_dbg_001",
         "Off-by-one: fill only indices 0..n-1 of a 3-int array; the loop must not write index 3. Fix the loop bound. Compile check uses -fsyntax-only; content check requires 'i < n'.",
         {"main.c": "int main(void) {\n    int a[3];\n    int n = 3;\n    for (int i = 0; i <= n; i++) {\n        a[i] = i;\n    }\n    return a[0];\n}\n"},
         {"main.c": "int main(void) {\n    int a[3];\n    int n = 3;\n    for (int i = 0; i < n; i++) {\n        a[i] = i;\n    }\n    return a[0];\n}\n"},
         contains_check("i < n", forbid=("i <= n",))),
        ("eb_dbg_002",
         "Null pointer dereference: guard ptr before reading *ptr. The program must exit 0 without crashing (the guard may return 0 early).",
         {"main.c": "int main(void) {\n    const char *ptr = 0;\n    int c = *ptr;\n    return c;\n}\n"},
         {"main.c": "int main(void) {\n    const char *ptr = 0;\n    if (ptr == 0) {\n        return 0;\n    }\n    int c = *ptr;\n    return c;\n}\n"},
         CHECK_RUN),
        ("eb_dbg_003",
         "Wrong comparison: sum_to(n) must include n (inclusive). Currently uses i < n exclusive — for sum_to(3) expect 6 not 3. Change to i <= n.",
         {"util.c": "int sum_to(int n) {\n    int s = 0;\n    for (int i = 1; i < n; i++) s += i;\n    return s;\n}\n",
          "util.h": "int sum_to(int n);\n",
          "main.c": "#include \"util.h\"\nint main(void) { return sum_to(3) == 6 ? 0 : 1; }\n"},
         {"util.c": "int sum_to(int n) {\n    int s = 0;\n    for (int i = 1; i <= n; i++) s += i;\n    return s;\n}\n",
          "util.h": "int sum_to(int n);\n",
          "main.c": "#include \"util.h\"\nint main(void) { return sum_to(3) == 6 ? 0 : 1; }\n"},
         '''#!/usr/bin/env python3
import subprocess, sys
from pathlib import Path
# compile util+main and run
src = Path("util.c").read_text(encoding="utf-8") if Path("util.c").is_file() else ""
if "i <= n" not in src:
    print("loop not inclusive")
    sys.exit(1)
files = [f for f in ("main.c", "util.c") if Path(f).is_file()]
r = subprocess.run(["gcc", "-std=c11", "-Werror=implicit-function-declaration", "-o", "tbin"] + files,
                   capture_output=True, text=True)
if r.returncode != 0:
    sys.stderr.write(r.stderr); sys.exit(1)
bin_path = Path("tbin.exe") if Path("tbin.exe").is_file() else Path("tbin")
r = subprocess.run([str(bin_path.resolve())])
sys.exit(r.returncode)
'''),
        ("eb_dbg_004",
         "Operator bug: clamp_upper must return hi when v > hi, but uses < . Fix the comparison so max is applied. The program must exit 0 (clamp_upper(20,10) == 10).",
         {"util.c": "int clamp_upper(int v, int hi) {\n    if (v < hi) return hi;\n    return v;\n}\nint main(void) { return clamp_upper(20, 10); }\n"},
         {"util.c": "int clamp_upper(int v, int hi) {\n    if (v > hi) return hi;\n    return v;\n}\nint main(void) { return clamp_upper(20, 10) == 10 ? 0 : 1; }\n"},
         CHECK_RUN),
        ("eb_dbg_005",
         "Uninitialized variable: total must start at 0 before the loop. Fix the code; require 'int total = 0'.",
         {"main.c": "int main(void) {\n    int total;\n    for (int i = 1; i <= 3; i++) total += i;\n    return total == 6 ? 0 : 1;\n}\n"},
         {"main.c": "int main(void) {\n    int total = 0;\n    for (int i = 1; i <= 3; i++) total += i;\n    return total == 6 ? 0 : 1;\n}\n"},
         contains_check("int total = 0", forbid=("    int total;\n",))),
        ("eb_dbg_006",
         "Logic error: is_valid should accept v in [0, 99] inclusive. Currently rejects 99 (uses v < 99). Fix so the program exits 0.",
         {"main.c": "static int is_valid(int v) {\n    return v >= 0 && v < 99;\n}\nint main(void) {\n    return is_valid(99) ? 0 : 1;\n}\n"},
         {"main.c": "static int is_valid(int v) {\n    return v >= 0 && v <= 99;\n}\nint main(void) {\n    return is_valid(99) ? 0 : 1;\n}\n"},
         CHECK_RUN),
        ("eb_dbg_007",
         "Buffer too small: the buffer must hold 6 chars + NUL (char buf[7] at least). Currently buf[4]. Fix the size so strcpy-like initialization is valid and the program exits 0. Forbidden: 'char buf[4]'.",
         {"main.c": "int main(void) {\n    char buf[4] = \"abcdef\";\n    return buf[0] == 'a' ? 0 : 1;\n}\n"},
         {"main.c": "int main(void) {\n    char buf[7] = \"abcdef\";\n    return buf[0] == 'a' ? 0 : 1;\n}\n"},
         run_and_contains(
             {"main.c": ("char buf[7]",)},
             forbid=(("main.c", "char buf[4]"),))),
    ]
    for tid, prompt, b, a, c in dbg:
        write_task(tid, "debug", prompt, b, a, c)
        tasks.append((tid, "debug"))

    # index.tsv
    lines = ["id\tcategory\tpath"]
    for tid, cat in tasks:
        lines.append(f"{tid}\t{cat}\t{cat}/{tid}")
    (BANK / "index.tsv").write_text("\n".join(lines) + "\n", encoding="utf-8")

    man = BANK / "MANIFEST.md"
    man.write_text(
        "# Engineering task bank\n\n"
        f"- Tasks: {len(tasks)}\n"
        "- Categories: compiler_repair, refactor, test_authoring, build_ci, docs, shell, multi_file, debug\n"
        "- Contract: see COORDINATION.md\n"
        "- Self-test: `python scripts/test_engineering_bank.py`\n"
        "- `after/` is golden end-state for self-test only; never show it to the agent under test.\n",
        encoding="utf-8")

    print(f"generated {len(tasks)} tasks under {BANK}")
    cats = sorted({c for _, c in tasks})
    print(f"categories ({len(cats)}): {', '.join(cats)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
