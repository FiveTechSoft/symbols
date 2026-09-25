#!/usr/bin/env python3
"""Engineering-bank harness: run an agent on every bank task and check it.

For each row of tests/fixtures/engineering_bank/index.tsv:
  1. copy the task's before/ into a fresh temp workdir;
  2. run the agent under test in that workdir with the task.md text;
  3. copy check.py in and run it (CWD = workdir); exit 0 = pass.
The golden after/ state is read only in --self-test mode (then it replaces
step 2, to prove every check can pass).

Optional per-task setup (for tasks that need state a plain file tree cannot
hold, such as a git repository with history):
  setup.py   run with CWD = workdir after before/ is copied and before the
             snapshot and the agent; a fixed git identity and fixed dates are
             in its environment so the history it builds is reproducible.
             A failing setup is a harness error (check_rc 125, setup_failed),
             never a pass or a wrong edit.
  golden.py  self-test only: run instead of copying after/ over the workdir,
             for golden states that are git operations (commit, branch, ...).
             Without it, a task with setup.py overlays after/ onto the
             workdir and keeps .git.
The snapshot skips .git/ contents and records the repository state (HEAD,
branches, status) as one "@git" entry, so a commit counts as a change.

Output: one JSON object on stdout (and --out FILE), with
  tasks_total, tasks_passed, pass_rate, pass_rate_by_category,
  wrong_edits (the agent changed files and the check still fails),
  untouched (the agent changed nothing), agent_errors, wall_ms_total,
  tasks: [{id, category, passed, check_rc, agent_rc, changed_files,
           wrong_edit, wall_ms, agent_ms}]
No task ids are hardcoded; everything comes from index.tsv.

Agent command: --agent is a shell-free template split on spaces; the
placeholders {workdir}, {task} (task text) and {task_file} are
substituted per task. Default: build/symbols-agent -w {workdir} {task}
--agent noop runs nothing (baseline: every check must fail).

Usage (repo root, after building):
  python3 tools/bank_harness.py                       # symbols-agent
  python3 tools/bank_harness.py --agent noop          # baseline
  python3 tools/bank_harness.py --self-test           # golden states
  python3 tools/bank_harness.py --min-pass-rate 0.1   # exit 1 below it
"""
from __future__ import annotations

import argparse
import re
import filecmp
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BANK = ROOT / "tests" / "fixtures" / "engineering_bank"
EXE = ".exe" if os.name == "nt" else ""
DEFAULT_AGENT = f"build/symbols-agent{EXE} -w {{workdir}} {{task}}"


def read_index(bank: Path) -> list[tuple[str, str, Path]]:
    rows = []
    for line in (bank / "index.tsv").read_text(encoding="utf-8").splitlines():
        parts = line.rstrip("\n").split("\t")
        if len(parts) < 3 or parts[0] in ("", "id") or parts[0].startswith("#"):
            continue
        rows.append((parts[0], parts[1], bank / parts[2]))
    return rows


def split_of(task_id: str) -> str:
    """Fixed 50/50 split by the parity of the id's trailing number:
    odd = dev (used while developing the agent), even = heldout."""
    digits = task_id.rsplit("_", 1)[-1]
    return "dev" if digits.isdigit() and int(digits) % 2 == 1 else "heldout"


GIT_ENV = {
    "GIT_AUTHOR_NAME": "Bank Setup", "GIT_AUTHOR_EMAIL": "bank@example.invalid",
    "GIT_COMMITTER_NAME": "Bank Setup", "GIT_COMMITTER_EMAIL": "bank@example.invalid",
    "GIT_AUTHOR_DATE": "2026-01-01T00:00:00+0000", "GIT_COMMITTER_DATE": "2026-01-01T00:00:00+0000",
    "GIT_CONFIG_NOSYSTEM": "1",
}


def git_state(d: Path) -> str:
    """HEAD, branches and status of the repository in d, as one string."""
    parts = []
    for args in (["rev-parse", "HEAD"], ["for-each-ref", "--format=%(refname) %(objectname)"],
                 ["status", "--porcelain"]):
        try:
            r = subprocess.run(["git", *args], cwd=d, capture_output=True, text=True,
                               encoding="utf-8", errors="replace", timeout=20)
            parts.append(r.stdout)
        except (OSError, subprocess.TimeoutExpired):
            parts.append("?")
    return hashlib.sha256("\0".join(parts).encode("utf-8")).hexdigest()


def snapshot(d: Path) -> dict[str, str]:
    out = {}
    for p in sorted(d.rglob("*")):
        rel = p.relative_to(d).as_posix()
        if rel == ".git" or rel.startswith(".git/"):
            continue
        if p.is_file():
            out[rel] = hashlib.sha256(p.read_bytes()).hexdigest()
    if (d / ".git").exists():
        out["@git"] = git_state(d)
    return out


def run_script(script: Path, wd: Path, timeout: int) -> int:
    env = dict(os.environ)
    env.update(GIT_ENV)
    try:
        r = subprocess.run([sys.executable, str(script)], cwd=wd, env=env, capture_output=True,
                           text=True, encoding="utf-8", errors="replace", timeout=timeout)
        return r.returncode
    except (OSError, subprocess.TimeoutExpired):
        return 124


def changed(before: dict[str, str], after: dict[str, str]) -> list[str]:
    keys = set(before) | set(after)
    return sorted(k for k in keys if before.get(k) != after.get(k))


def run_agent(template: str, workdir: Path, task_text: str, task_file: Path,
              timeout: int) -> tuple[int, float, str]:
    if template == "noop":
        return 0, 0.0, ""
    argv = []
    for tok in template.split(" "):
        if tok == "":
            continue
        argv.append(tok.replace("{workdir}", str(workdir))
                       .replace("{task_file}", str(task_file))
                       .replace("{task}", task_text))
    if argv and not os.path.isabs(argv[0]) and (ROOT / argv[0]).exists():
        argv[0] = str(ROOT / argv[0])
    t0 = time.perf_counter()
    try:
        r = subprocess.run(argv, cwd=workdir, capture_output=True, text=True,
                           encoding="utf-8", errors="replace", timeout=timeout)
        rc, log = r.returncode, (r.stdout or "") + (r.stderr or "")
    except subprocess.TimeoutExpired:
        rc, log = 124, "agent timeout"
    except OSError as e:
        rc, log = 127, f"agent not runnable: {e}"
    return rc, (time.perf_counter() - t0) * 1000.0, log


TASK_TEXT_OVERRIDE = None

# Closed vocabulary for --reasons: an abstention reason is reported only as one
# of these fixed classes, so no workspace-derived text (names, paths, numbers)
# can leave a blind run.
REASON_CLASSES = [
    ("ambiguous: single edits", r"ambiguous: \d+ single edits"),
    ("ambiguous: relational swaps", r"ambiguous: \d+ relational swaps"),
    ("ambiguous: fragment places", r"ambiguous: \d+ places fit"),
    ("ambiguous: dead functions", r"ambiguous: \d+ dead functions"),
    ("ambiguous: doc terms", r"ambiguous: \d+ doc terms"),
    ("ambiguous: rename candidates", r"ambiguous: \d+ rename candidates"),
    ("ambiguous: deleted files", r"ambiguous: \d+ deleted files"),
    ("abstain: demoted induced pattern", r"abstain: every verified swap"),
    ("no operator preconditions hold", r"no operator preconditions hold"),
    ("git: merge not actionable", r"merge (in progress|conflict)"),
    ("git: restore not asked", r"deleted file named but"),
    ("git: revert preconditions", r"revert: needs"),
    ("verify failed", r"verify failed"),
    ("write failed", r"write failed"),
]

# Closed vocabulary of the first compiler error class (task_ops diag_shape).
DIAG_CLASSES = ["none", "link", "missing_header", "implicit", "undeclared", "unknown_type",
                "conflicting_types", "arity", "no_member", "redefinition", "incompatible",
                "invalid_operands", "lvalue", "incomplete_type", "expected", "return", "other"]


# Closed vocabulary of operator names (task_ops.c op_names[] plus git_ops). A
# name outside this list is never echoed; the tag is dropped instead.
OP_NAMES = ["author_test", "rename_symbol", "stated_fragment", "literal_to_constant", "declare_implicit",
            "compiler_fixit", "doc_sync", "remove_dead_function", "unmatched_brace", "relop_search",
            "shell_harden", "build_repair", "c_fix", "compile_repair", "shell_contract", "c_contract",
            "evidence_fix", "resolve_merge", "restore_deleted", "revert_head"]


def op_tag(log: str, kept_line: str) -> str:
    """Closed-form tag for a verify failure: which operator's edit was rolled
    back, whether its intent check held, and whether the edit was outside the
    file the task names. Only fixed tokens are emitted, never task text."""
    ops = re.findall(r"Operator (\w+): .*\(rolled back\)", log)
    if not ops or ops[-1] not in OP_NAMES:
        return ""
    tag = " [op=" + ops[-1]
    m = re.match(r"verify failed: intent=([01]) compile -?\d+->-?\d+ run -?\d+->-?\d+"
                 r"( \(the edit is outside the file the task names\))?", kept_line)
    if m:
        tag += " intent=%s scope=%s" % (m.group(1), "0" if m.group(2) else "1")
    return tag + "]"


def reason_class(log: str, agent_rc: int) -> str:
    if agent_rc in (124, 127):
        return "agent error"
    kept = re.findall(r"No edit kept: (.*)", log)
    rolled = re.findall(r"Operator (\w+): .*\(rolled back\)", log)
    if kept:
        m = re.search(r"no operator preconditions hold \[(c=(?:-1|0|1) run=(?:-1|0|1) sh=[01] mk=[01] "
                      r"doc=[01] test=[01] git=[01])\]", kept[-1])
        if m:
            d = re.search(r"git=[01]\] \[diag=(" + "|".join(DIAG_CLASSES) + r") nerr=([124]) nc=([12])"
                          r"(?: cr=([012]) cb=([012])| crskip=(named|nosrc|noerr|notrun))?\]", kept[-1])
            tail = ""
            if d:
                tail = " [diag=%s nerr=%s nc=%s" % d.groups()[:3]
                tail += (" cr=%s cb=%s" % d.groups()[3:5]) if d.group(4) is not None else ""
                tail += (" crskip=%s" % d.group(6)) if d.group(6) is not None else ""
                tail += "]"
            k = re.search(r"git=[01]\] \[cc=([01])(?: cn=([012]) cp=([012])| ccw=(nogoal|now|exit|file|noval|multi))?\]", kept[-1])
            if k:
                tail = " [cc=%s" % k.group(1)
                tail += (" cn=%s cp=%s" % k.groups()[1:3]) if k.group(2) is not None else ""
                tail += (" ccw=%s" % k.group(4)) if k.group(4) is not None else ""
                tail += "]"
            return "no operator preconditions hold [" + m.group(1) + "]" + tail
        for name, rx in REASON_CLASSES:
            if re.search(rx, kept[-1]):
                return name + (op_tag(log, kept[-1]) if name == "verify failed" else "")
        return "other reason"
    if rolled:
        return "rolled back (operator verify failed)" + op_tag(log, "")
    if re.search(r"Operator \w+: .*\(verified, kept\)", log):
        return "edit kept but check failed"
    return "no reason line"


def run_task(tid: str, cat: str, tdir: Path, agent: str, self_test: bool,
             timeout: int) -> dict:
    t0 = time.perf_counter()
    task_md = tdir / "task.md"
    task_text = task_md.read_text(encoding="utf-8").strip() if task_md.is_file() else ""
    if TASK_TEXT_OVERRIDE is not None:
        task_text = TASK_TEXT_OVERRIDE
    with tempfile.TemporaryDirectory(prefix="bank_") as td:
        wd = Path(td) / "work"
        shutil.copytree(tdir / "before", wd)
        has_setup = (tdir / "setup.py").is_file()
        setup_failed = has_setup and run_script(tdir / "setup.py", wd, timeout) != 0
        agent_rc, agent_ms, log, diff = 0, 0.0, "", []
        if setup_failed:
            check_rc = 125
        else:
            snap0 = snapshot(wd)
            if self_test and (tdir / "golden.py").is_file():
                if run_script(tdir / "golden.py", wd, timeout) != 0:
                    agent_rc = 1
            elif self_test and has_setup:
                shutil.copytree(tdir / "after", wd, dirs_exist_ok=True)
            elif self_test:
                shutil.rmtree(wd)
                shutil.copytree(tdir / "after", wd)
            else:
                agent_rc, agent_ms, log = run_agent(agent, wd, task_text, task_md, timeout)
            diff = changed(snap0, snapshot(wd))
            shutil.copy2(tdir / "check.py", wd / "check.py")
            try:
                r = subprocess.run([sys.executable, "check.py"], cwd=wd, capture_output=True,
                                   text=True, encoding="utf-8", errors="replace", timeout=timeout)
                check_rc = r.returncode
            except subprocess.TimeoutExpired:
                check_rc = 124
    passed = check_rc == 0
    return {
        "id": tid,
        "category": cat,
        "passed": passed,
        "check_rc": check_rc,
        "agent_rc": agent_rc,
        "changed_files": diff,
        "wrong_edit": bool(diff) and not passed,
        "setup_failed": setup_failed,
        "wall_ms": round((time.perf_counter() - t0) * 1000.0, 1),
        "agent_ms": round(agent_ms, 1),
        "agent_log_tail": log[-300:] if (not passed and log) else "",
        "reason": "" if passed else reason_class(log, agent_rc),
    }


def summarize(tasks: list[dict], agent: str, self_test: bool) -> dict:
    by_cat: dict[str, list[int]] = {}
    for t in tasks:
        c = by_cat.setdefault(t["category"], [0, 0])
        c[0] += 1
        c[1] += 1 if t["passed"] else 0
    total = len(tasks)
    passed = sum(1 for t in tasks if t["passed"])
    return {
        "schema": 1,
        "mode": "self-test" if self_test else "agent",
        "agent": "golden after/" if self_test else agent,
        "tasks_total": total,
        "tasks_passed": passed,
        "pass_rate": round(passed / total, 4) if total else 0.0,
        "pass_rate_by_category": {k: round(v[1] / v[0], 4) for k, v in sorted(by_cat.items())},
        "passed_by_category": {k: f"{v[1]}/{v[0]}" for k, v in sorted(by_cat.items())},
        "wrong_edits": sum(1 for t in tasks if t["wrong_edit"]),
        "untouched": sum(1 for t in tasks if not t["changed_files"]),
        "agent_errors": sum(1 for t in tasks if t["agent_rc"] in (124, 127)),
        "setup_failures": sum(1 for t in tasks if t.get("setup_failed")),
        "wall_ms_total": round(sum(t["wall_ms"] for t in tasks), 1),
        "passed_by_split": {sp: f"{sum(1 for t in tasks if t['passed'] and split_of(t['id']) == sp)}"
                                f"/{sum(1 for t in tasks if split_of(t['id']) == sp)}"
                            for sp in ("dev", "heldout")},
        "tasks": tasks,
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--agent", default=DEFAULT_AGENT)
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--bank", default=str(BANK))
    ap.add_argument("--timeout", type=int, default=60)
    ap.add_argument("--out")
    ap.add_argument("--min-pass-rate", type=float)
    ap.add_argument("--max-wrong-edits", type=int)
    ap.add_argument("--split", choices=("all", "dev", "heldout"), default="all")
    ap.add_argument("--counts-only", action="store_true",
                    help="omit per-task rows (ids, files) from the output; for blind banks")
    ap.add_argument("--task-text",
                    help="replace every task.md with this text (wording-robustness probe: what the "
                         "agent can do from the code's own evidence alone)")
    ap.add_argument("--reasons", action="store_true",
                    help="add counts of failed tasks per closed-vocabulary abstention reason, "
                         "by category (safe with --counts-only)")
    a = ap.parse_args()
    global TASK_TEXT_OVERRIDE
    TASK_TEXT_OVERRIDE = a.task_text
    rows = [r for r in read_index(Path(a.bank)) if a.split == "all" or split_of(r[0]) == a.split]
    if not rows:
        print("no tasks in index.tsv", file=sys.stderr)
        return 2
    tasks = [run_task(tid, cat, tdir, a.agent, a.self_test, a.timeout) for tid, cat, tdir in rows]
    res = summarize(tasks, a.agent, a.self_test)
    if a.reasons:
        rc_: dict[str, dict[str, int]] = {}
        for t in tasks:
            if not t["passed"]:
                d = rc_.setdefault(t["category"], {})
                d[t["reason"]] = d.get(t["reason"], 0) + 1
        res["reasons_by_category"] = dict(sorted(rc_.items()))
        tot: dict[str, int] = {}
        for d in rc_.values():
            for k, v in d.items():
                tot[k] = tot.get(k, 0) + v
        res["reasons_total"] = dict(sorted(tot.items(), key=lambda kv: -kv[1]))
        flags: dict[str, dict[str, int]] = {}
        for t in tasks:
            for kv in re.findall(r"(\w+)=(-?\d|" + "|".join(DIAG_CLASSES) + r")\b", t["reason"]):
                d = flags.setdefault(kv[0], {})
                d[kv[1]] = d.get(kv[1], 0) + 1
        res["abstain_shape_flags"] = flags
    if a.counts_only:
        res.pop("tasks", None)
    text = json.dumps(res, ensure_ascii=False)
    if a.out:
        Path(a.out).write_text(text + "\n", encoding="utf-8")
    print(text)
    s = {k: res[k] for k in ("mode", "tasks_total", "tasks_passed", "pass_rate", "wrong_edits", "untouched", "agent_errors", "setup_failures")}
    s.update({f"pass_{k}": v for k, v in res["passed_by_split"].items()})
    print("BANK_HARNESS " + " ".join(f"{k}={v}" for k, v in s.items()), file=sys.stderr)
    rc = 0
    if a.min_pass_rate is not None and res["pass_rate"] < a.min_pass_rate:
        rc = 1
    if a.max_wrong_edits is not None and res["wrong_edits"] > a.max_wrong_edits:
        rc = 1
    if res["setup_failures"]:
        rc = 1
    return rc


if __name__ == "__main__":
    sys.exit(main())
