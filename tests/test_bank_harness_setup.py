#!/usr/bin/env python3
"""test_bank_harness_setup.py: per-task setup.py / golden.py in tools/bank_harness.py.

Builds a throwaway bank with three tasks and runs the harness on it:
  git_commit  setup.py builds a one-commit repo; golden.py commits a change;
              the check wants two commits and a clean tree
  git_overlay setup.py builds a repo; after/ is overlaid and .git must survive
  bad_setup   setup.py fails: a harness error, never a pass or a wrong edit
Also: setup is reproducible (same HEAD twice), a noop agent leaves every git
task untouched, and a commit shows up as the "@git" change.
"""
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HARNESS = ROOT / "tools" / "bank_harness.py"
fails = 0


def check(cond, what):
    global fails
    print(("ok   " if cond else "FAIL ") + what)
    if not cond:
        fails += 1


SETUP_REPO = '''import subprocess
def g(*a): subprocess.run(["git", *a], check=True, capture_output=True)
g("init", "-q"); g("config", "user.useConfigOnly", "false")
g("add", "a.txt"); g("commit", "-q", "-m", "first")
'''
GOLDEN_COMMIT = '''import subprocess
open("a.txt", "w").write("v2\\n")
subprocess.run(["git", "commit", "-q", "-am", "second"], check=True, capture_output=True)
'''
CHECK_TWO_COMMITS = '''import subprocess, sys
n = subprocess.run(["git", "rev-list", "--count", "HEAD"], capture_output=True, text=True).stdout.strip()
st = subprocess.run(["git", "status", "--porcelain", "--", "a.txt"], capture_output=True, text=True).stdout.strip()
sys.exit(0 if n == "2" and st == "" else 1)
'''
CHECK_OVERLAY = '''import os, sys
sys.exit(0 if open("a.txt").read() == "v2\\n" and os.path.isdir(".git") else 1)
'''


def task(bank, tid, files):
    d = bank / tid
    for rel, text in files.items():
        p = d / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text)
    return f"{tid}\tgit\t{tid}\n"


def run(bank, *args):
    r = subprocess.run([sys.executable, str(HARNESS), "--bank", str(bank), *args],
                       capture_output=True, text=True)
    return r.returncode, json.loads(r.stdout.strip().splitlines()[-1])


def main():
    if shutil.which("git") is None:
        print("git not found: skip")
        return 77
    with tempfile.TemporaryDirectory(prefix="bank_setup_") as td:
        bank = Path(td)
        idx = "id\tcategory\tpath\n"
        idx += task(bank, "git_commit", {"before/a.txt": "v1\n", "after/a.txt": "v2\n", "task.md": "Commit the change.",
                                         "setup.py": SETUP_REPO, "golden.py": GOLDEN_COMMIT, "check.py": CHECK_TWO_COMMITS})
        idx += task(bank, "git_overlay", {"before/a.txt": "v1\n", "after/a.txt": "v2\n", "task.md": "Change a.txt.",
                                          "setup.py": SETUP_REPO, "check.py": CHECK_OVERLAY})
        idx += task(bank, "bad_setup", {"before/a.txt": "v1\n", "after/a.txt": "v2\n", "task.md": "x",
                                        "setup.py": "import sys; sys.exit(3)\n", "check.py": "import sys; sys.exit(0)\n"})
        (bank / "index.tsv").write_text(idx)

        rc, res = run(bank, "--self-test")
        t = {x["id"]: x for x in res["tasks"]}
        check(t["git_commit"]["passed"], "golden.py commit passes the git check")
        check("@git" in t["git_commit"]["changed_files"] and "a.txt" in t["git_commit"]["changed_files"],
              "a commit is recorded as @git plus the file change")
        check(not any(f.startswith(".git/") for x in res["tasks"] for f in x["changed_files"]), ".git internals never listed")
        check(t["git_overlay"]["passed"], "after/ overlay keeps .git")
        check(not t["bad_setup"]["passed"] and t["bad_setup"]["check_rc"] == 125 and t["bad_setup"]["setup_failed"]
              and not t["bad_setup"]["wrong_edit"], "failing setup: harness error, not pass, not wrong edit")
        check(res["setup_failures"] == 1 and rc == 1, "setup failure counted and makes the harness exit 1")

        rc, res = run(bank, "--agent", "noop")
        t = {x["id"]: x for x in res["tasks"]}
        check(not t["git_commit"]["passed"] and t["git_commit"]["changed_files"] == [], "noop agent: git task untouched and failing")

        heads = []
        for i in range(2):
            wd = bank / f"repro{i}"
            shutil.copytree(bank / "git_commit" / "before", wd)
            sys.path.insert(0, str(HARNESS.parent))
            import bank_harness
            check(bank_harness.run_script(bank / "git_commit" / "setup.py", wd, 60) == 0, f"setup run {i} succeeds")
            heads.append(subprocess.run(["git", "rev-parse", "HEAD"], cwd=wd, capture_output=True, text=True).stdout.strip())
        check(heads[0] and heads[0] == heads[1], "setup history is reproducible (same HEAD)")
    # verify-failed reasons carry only a closed-form operator tag
    sys.path.insert(0, str(HARNESS.parent))
    import bank_harness as bh
    rb = "[symbols-agent] Operator %s: detail with task text (rolled back)\n"
    vf = "[symbols-agent] No edit kept: verify failed: intent=%s compile 1->1 run 0->0%s\n"
    out = " (the edit is outside the file the task names)"
    check(bh.reason_class(rb % "c_contract" + vf % ("0", out), 0) == "verify failed [op=c_contract intent=0 scope=0]",
          "verify failed tags operator, intent and scope")
    check(bh.reason_class(rb % "c_fix" + vf % ("1", ""), 0) == "verify failed [op=c_fix intent=1 scope=1]",
          "verify failed in scope")
    check(bh.reason_class(rb % "not_an_op" + vf % ("0", ""), 0) == "verify failed", "unknown operator name is dropped")
    check(bh.reason_class(rb % "c_fix" + vf % ("1", " task text"), 0) == "verify failed [op=c_fix intent=1 scope=1]",
          "trailing text never echoed")
    check(bh.reason_class(rb % "resolve_merge" + "[symbols-agent] No edit kept: verify failed: merge not completed cleanly\n", 0)
          == "verify failed [op=resolve_merge]", "git operator tag without intent fields")
    check(bh.reason_class(rb % "c_fix", 0) == "rolled back (operator verify failed) [op=c_fix]", "rolled back tag")
    ab = "[symbols-agent] No edit kept: no operator preconditions hold [c=0 run=-1 sh=0 mk=0 doc=0 test=0 git=0] [diag=link nerr=1 nc=1%s]\n"
    check(bh.reason_class(ab % " crskip=named", 0).endswith("[diag=link nerr=1 nc=1 crskip=named]"), "crskip reason kept")
    check(bh.reason_class(ab % " crskip=other", 0).endswith("git=0]"), "unknown crskip drops the tail")
    check(bh.reason_class(ab % " cr=2 cb=0", 0).endswith("[diag=link nerr=1 nc=1 cr=2 cb=0]"), "cr/cb still kept")
    print("test_bank_harness_setup: " + ("ALL PASSED" if not fails else f"{fails} FAILED"))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
