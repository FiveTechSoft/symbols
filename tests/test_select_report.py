#!/usr/bin/env python3
"""S2a: `symbols-agent --select-report BUILD --changed FILE...` is a read-only
report of the build-graph test selection. Run in the built tree: argv[1] is the
symbols-agent binary, argv[2] the build directory. Checks:
  report_equals_select   the report body equals tools/build_graph.py select, read
                         in this process, for 5 changed-file sets (subset and full)
  fail_closed            every way the selection cannot be produced prints
                         "mode: full" and a reason: no script, no python3, no File
                         API reply, no changed file, a path outside the alphabet
  no_shell_injection     a path with shell syntax is refused and never reaches a shell
  modes_unchanged        exit codes and stderr of the existing modes, measured on the
                         binary before this change, are the same
The File API reply is made by re-running cmake on the build directory when it is
missing (same as test_build_graph_repo). Windows prints SKIP: this report is not
measured there. Nothing here claims the selection is complete; that is the sidecar's
and the strace comparison's limit."""
import json, os, subprocess, sys, tempfile, shutil
HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(SRC, "tools"))
import build_graph as bg

HEADER = "[symbols-agent] select-report (read-only: nothing is skipped, the full gate stays the default)"
fails = []


def check(name, ok, detail=""):
    if not ok:
        fails.append(name)
        print("FAIL", name, detail)


def run(agent, args, env=None, cwd=SRC):
    p = subprocess.run([agent] + args, cwd=cwd, env=env, capture_output=True, text=True, timeout=120)
    return p.returncode, p.stdout, p.stderr


def main():
    if os.name == "nt":
        print("SKIP: the select report is not measured on Windows")
        return 77
    agent, build = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    if not os.path.isdir(os.path.join(build, ".cmake", "api", "v1", "reply")):
        assert bg.configure(SRC, build) == 0, "cmake re-run for the File API reply failed"
    g = bg.load_graph(build)
    tmp = tempfile.mkdtemp()
    try:
        sets = [["README.md"], ["tests/test_i18n.c"], ["src/relation.c"], ["CMakeLists.txt"],
                ["README.md", "tests/test_i18n.c"]]
        modes = set()
        for ch in sets:
            rc, out, err = run(agent, ["-w", SRC, "--select-report", build, "--changed"] + ch)
            lines = out.split("\n", 1)
            check("report_equals_select", rc == 0 and lines[0] == HEADER and not err, "%s rc=%s" % (ch, rc))
            try:
                body = json.loads(lines[1])
            except ValueError:
                body = None
            exp = bg.select(g, ch)
            check("report_equals_select", body == exp, "%s: report != select" % ch)
            modes.add(exp["mode"])
        check("report_equals_select", modes == {"subset", "full"}, "modes seen %s" % sorted(modes))

        def full(name, args, env=None, why=None, cwd=SRC):
            rc, out, err = run(agent, args, env, cwd)
            ok = rc == 0 and out.startswith(HEADER + "\nmode: full\nreason: ") and (why is None or why in out)
            check(name, ok, "rc=%s out=%r err=%r" % (rc, out[:200], err[:200]))

        empty = os.path.join(tmp, "ws")
        os.mkdir(empty)
        full("fail_closed", ["-w", empty, "--select-report", build, "--changed", "README.md"],
             why="tools/build_graph.py was not found")
        check("fail_closed", os.listdir(empty) == [], "the workspace was written to")
        nopy = os.path.join(tmp, "nopath")
        os.mkdir(nopy)
        full("fail_closed", ["-w", SRC, "--select-report", build, "--changed", "README.md"],
             env={"PATH": nopy}, why="did not run to a successful exit")
        nobuild = os.path.join(tmp, "nobuild")
        os.mkdir(nobuild)
        rc, out, err = run(agent, ["-w", SRC, "--select-report", nobuild, "--changed", "README.md"])
        try:
            body = json.loads(out.split("\n", 1)[1]) if rc == 0 and out.startswith(HEADER) else {}
        except ValueError:
            body = {}
        check("fail_closed", body.get("mode") == "full" and body.get("tests") == [] and
              any("no File API reply" in r for r in body.get("reasons", [])), "no-reply: %r" % out[:200])
        check("fail_closed", os.listdir(nobuild) == [], "the build dir was written to")
        full("fail_closed", ["-w", SRC, "--select-report", build, "--changed"], why="no changed files")
        for bad in ["a b", "a;b", "$(x)", "a`b`", "a\nb", "é.c"]:
            full("fail_closed", ["-w", SRC, "--select-report", build, "--changed", "README.md", bad],
                 why="outside the supported set")
        marker = os.path.join(tmp, "marker")
        full("no_shell_injection", ["-w", SRC, "--select-report", build, "--changed",
                                    "x;touch", marker], why="outside the supported set")
        for body, tag in [("print('hello')", "not JSON"), ("print('{ }')", "no mode"), ("print('x \"mode\": \"subset\"')", "mode but not JSON"),
                          ("import sys; print('{\"mode\": \"subset\"}'); sys.exit(3)", "exit 3")]:
            fake = os.path.join(tmp, "fake")
            shutil.rmtree(fake, ignore_errors=True)
            os.makedirs(os.path.join(fake, "tools"))
            open(os.path.join(fake, "tools", "build_graph.py"), "w").write(body + "\n")
            full("fail_closed", ["-w", fake, "--select-report", build, "--changed", "README.md"],
                 why="did not run" if tag == "exit 3" else "not recognised")
        check("no_shell_injection", not os.path.exists(marker), "the injected command ran")
        full("fail_closed", ["-w", SRC, "--select-report", "bad dir;x", "--changed", "README.md"],
             why="build directory path")
        full("fail_closed", ["-w", SRC, "--select-report", "--help", "--changed", "README.md"],
             why="build directory path")

        rc, out, err = run(agent, [])
        rc2, out2, err2 = run(agent, ["-h"])
        check("modes_unchanged", rc == 0 and rc2 == 0 and out == out2 and "--select-report" in out and
              "--continue-stdout-goal" in out and "--ask-missing-goal" in out, "help")
        msg_cont = "[symbols-agent] Continuation refused: exact typed request, key and single-line answer required.\n"
        msg_flags = "[symbols-agent] Continuation flags require --continue-stdout-goal.\n"
        msg_sel = "[symbols-agent] --select-report needs --changed and cannot be combined with other modes.\n"
        for args, want in [(["--bogus"], msg_flags), (["--continue-stdout-goal"], msg_cont),
                           (["--workspace-key"], msg_flags), (["--stdout-answer", "x"], msg_flags),
                           (["--select-report", build], msg_sel), (["--changed", "README.md"], msg_sel),
                           (["--select-report", build, "--changed", "README.md", "-i"], msg_sel)]:
            rc, out, err = run(agent, args)
            check("modes_unchanged", rc == 1 and out == "" and err == want, "%s rc=%s err=%r" % (args, rc, err))
        for ch in sets[:1]:
            print("report ok for", ch, "subset size", len(bg.select(g, ch)["tests"]))
        print("failed checks:", " ".join(sorted(set(fails))) or "none")
        if not fails:
            print("select_report test ok")
        return 1 if fails else 0
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
