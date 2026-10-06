#!/usr/bin/env python3
"""S2b: the opt-in select-report hook in TaskOpsSolve. Run: argv[1] is the
symbols-agent binary. A real rename task runs on a temp workspace whose
tools/build_graph.py is a fake that prints its arguments as JSON, so the test
checks what the hook asks for, not what the selection is (test_select_report
covers that). Checks:
  gate_off          SYMBOLS_SELECT_REPORT unset or "0": stderr is empty
  report_on_verify  gate on, verified rename of a.c and b.c: one report on stderr
                    that names exactly those two files and the build directory
  stdout_files_same stdout (time and task-id lines removed) and the edited files
                    are byte-identical with the gate on and off
  fail_closed       missing script, a script that exits 3, an unsafe build
                    directory: one "mode: full" line with a reason, same stdout,
                    same exit code, same edited files
  no_report_unverified  a task that changes nothing prints no report (an attempt that
                    writes and is then refuted is NOT covered: no fixture made one)
Windows prints SKIP. Not covered: operators other than rename (they do not
record files and report "mode: full"; checked by a source mutant, not here)."""
import json, os, re, shutil, subprocess, sys, tempfile

HEADER = "[symbols-agent] select-report for the verified edit"
fails = []


def check(name, ok, detail=""):
    if not ok:
        fails.append(name)
        print("FAIL", name, detail)


FAKE = ("import json, sys\n"
        "if sys.argv[1:2] != ['select']: sys.exit(2)\n"
        "print(json.dumps({'mode': 'subset', 'argv': sys.argv[2:]}, indent=2))\n")


def make_ws(root, script):
    os.makedirs(root + "/tools")
    for n, t in (("a.c", "#define OLD_LIMIT 4\nint a(void){return OLD_LIMIT;}\n"),
                 ("b.c", "extern int a(void);\nint b(void){return OLD_LIMIT+a();}\n"),
                 ("main.c", "int main(void){return 0;}\n")):
        open(root + "/" + n, "w").write(t)
    if script is not None:
        open(root + "/tools/build_graph.py", "w").write(script)


def norm(s):
    return "".join(l for l in s.splitlines(True)
                   if not re.match(r"(Resolution Time|Task ID):|\[symbols-agent\] Indexed ", l)).replace(
        re.search(r"Target Workspace: '([^']*)'", s).group(1) if "Target Workspace" in s else "\0", "WS")


def solve(agent, task, gate, script=FAKE):
    d = tempfile.mkdtemp()
    ws = d + "/ws"
    make_ws(ws, script)
    env = dict(os.environ)
    env.pop("SYMBOLS_SELECT_REPORT", None)
    if gate is not None:
        env["SYMBOLS_SELECT_REPORT"] = gate
    p = subprocess.run([agent, "-w", ws, task], cwd=d, env=env, capture_output=True, text=True, timeout=120)
    files = {n: open(ws + "/" + n).read() for n in ("a.c", "b.c", "main.c")}
    shutil.rmtree(d, ignore_errors=True)
    return p.returncode, norm(p.stdout), p.stderr, files


def main():
    if os.name == "nt":
        print("SKIP: the select hook is not measured on Windows")
        return 77
    agent = sys.argv[1]
    task = "Rename OLD_LIMIT to NEW_LIMIT"
    base = solve(agent, task, None)
    check("baseline_edit", "NEW_LIMIT" in base[3]["a.c"] and "NEW_LIMIT" in base[3]["b.c"]
          and base[3]["main.c"] == "int main(void){return 0;}\n" and base[0] == 0, repr(base[3]))
    check("gate_off_unset", base[2] == "", repr(base[2]))
    zero = solve(agent, task, "0")
    check("gate_off_zero", zero[2] == "" and zero[:2] == base[:2] and zero[3] == base[3], repr(zero[2]))
    on = solve(agent, task, "bdir")
    lines = on[2].splitlines()
    check("report_header", len(lines) >= 2 and lines[0].startswith(HEADER), repr(on[2]))
    body = on[2][on[2].find("{"):]
    try:
        argv = json.loads(body)["argv"]
    except Exception as e:
        argv = None
        check("report_json", False, repr(on[2]))
    check("report_names_files", argv is not None and argv[0] == "bdir" and argv[1] == "--changed"
          and sorted(argv[2:]) == ["a.c", "b.c"], repr(argv))
    check("report_once", on[2].count(HEADER) == 1, repr(on[2]))
    check("stdout_files_same", on[0] == base[0] and on[1] == base[1] and on[3] == base[3],
          "rc %s/%s" % (on[0], base[0]))
    bad = [("missing_script", "bdir", None), ("script_exit3", "bdir", "import sys\nsys.exit(3)\n"),
           ("unsafe_build", "b;d", FAKE), ("garbage_output", "bdir", "print('hello')\n")]
    for name, gate, script in bad:
        r = solve(agent, task, gate, script)
        check(name + "_full", r[2].count("mode: full") == 1 and "reason:" in r[2] and r[2].count(HEADER) == 1, repr(r[2]))
        check(name + "_same", r[0] == base[0] and r[1] == base[1] and r[3] == base[3])
    nov = solve(agent, "Rename MISSING_NAME to OTHER_NAME", "bdir")
    check("no_report_unverified", HEADER not in nov[2] and "mode:" not in nov[2], repr(nov[2]))
    check("unverified_files_untouched", nov[3] == solve(agent, "Rename MISSING_NAME to OTHER_NAME", None)[3])
    if fails:
        print("FAILED", sorted(set(fails)))
        return 1
    print("PASS select_hook")
    return 0


if __name__ == "__main__":
    sys.exit(main())
