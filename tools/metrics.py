#!/usr/bin/env python3
"""Progress metrics measured from real runs (nothing estimated by hand).

Usage (from the repo root, with the build in ./build):
    python3 tools/metrics.py            # measure and print the table
    python3 tools/metrics.py --write    # also rewrite the README section
                                        # (between <!-- METRICS:BEGIN --> and <!-- METRICS:END -->)
                                        # and append a row to tools/metrics_history.csv
    python3 tools/metrics.py --ci       # include the GitHub CI status for the commit

Anything that cannot be measured here is reported as "not measured", never invented.
The fixed task bank also has scripts/bank_report.py (per-commit JSONL, CI workflow `bank`),
which parses the same runner METRICS lines.
"""
import csv, json, os, re, shutil, socket, statistics, subprocess, sys, tempfile, time
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = Path(os.environ.get("SYMBOLS_BUILD", ROOT / "build"))
EXE = ".exe" if os.name == "nt" else ""
REPO = "FiveTechSoft/symbols"


def run(cmd, cwd=ROOT, timeout=900, env=None):
    p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, encoding="utf-8",
                       errors="replace", timeout=timeout, env=env)
    return p.returncode, (p.stdout or "") + (p.stderr or "")


def git(*a):
    return run(["git", *a])[1].strip()


# 1. CTest suite + unit asserts ------------------------------------------------
def m_ctest():
    rc, out = run(["ctest", "--test-dir", str(BUILD), "-V", "-j4"], timeout=1800)
    total = re.search(r"tests passed, (\d+) tests failed out of (\d+)", out)
    skipped = len(re.findall(r"\*\*\*Skipped", out))
    failed = int(total.group(1)) if total else None
    n = int(total.group(2)) if total else None
    ap = af = 0
    for a, b in re.findall(r"TEST RESULTS: (\d+) passed, (\d+) failed", out):
        ap += int(a); af += int(b)
    return {"ctest_total": n, "ctest_failed": failed, "ctest_skipped": skipped,
            "ctest_passed": (n - failed - skipped) if n is not None else None,
            "asserts_passed": ap, "asserts_failed": af}


# 2. Autonomous agent: C repair on separated fixtures --------------------------
def metrics_lines(text):
    rows = []
    for line in text.splitlines():
        if "METRICS" in line:
            rows.append(dict(re.findall(r"(\w+)=([\w.]+)", line)))
    return rows


def m_runner():
    r = {}
    for name in ("test_agent_runner_heldout", "test_agent_runner_external"):
        exe = BUILD / (name + EXE)
        if not exe.is_file():
            continue
        _, out = run([str(exe)])
        for row in metrics_lines(out):
            key = row.get("suite", "heldout")
            r[key] = {k: row.get(k) for k in ("cases", "resolved", "resolution_rate",
                                                "false_positives", "correct_abstentions")}
    return r


# 3. Server: grounded QA, latency and procedural memory ---------------------
def free_port():
    s = socket.socket(); s.bind(("127.0.0.1", 0)); p = s.getsockname()[1]; s.close(); return p


def post(port, body, timeout=30):
    req = urllib.request.Request(f"http://127.0.0.1:{port}/v1/chat/completions",
                                 data=json.dumps(body).encode(), headers={"content-type": "application/json"})
    t = time.perf_counter()
    with urllib.request.urlopen(req, timeout=timeout) as f:
        data = json.loads(f.read().decode("utf-8", "replace"))
    return data, (time.perf_counter() - t) * 1000.0


def start_server(procfile):
    exe = BUILD / ("symbols-server" + EXE)
    if not exe.is_file():
        return None, None
    port = free_port()
    env = dict(os.environ, SYMBOLS_PROCEDURAL=procfile)
    p = subprocess.Popen([str(exe), str(port)], cwd=BUILD, env=env,
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(50):
        try:
            socket.create_connection(("127.0.0.1", port), timeout=0.2).close(); return p, port
        except OSError:
            time.sleep(0.1)
    p.kill(); return None, None


ABSTAIN_RE = re.compile(r"^\s*(I don't know|No lo s[eé])", re.I)


def m_qa(port):
    rows = []
    for line in (ROOT / "tools/metrics/qa_battery.tsv").read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        parts = line.split("\t") + ["", ""]
        rows.append((parts[0], parts[1], [w for w in parts[2].split("|") if w]))
    tp = fp = fn = tn = wrong = 0
    lat, detail = [], []
    for q, exp, words in rows:
        data, ms = post(port, {"model": "symbols", "messages": [{"role": "user", "content": q}]})
        lat.append(ms)
        ans = (data.get("choices") or [{}])[0].get("message", {}).get("content") or ""
        abst = bool(ABSTAIN_RE.match(ans))
        ok_words = all(w.lower() in ans.lower() for w in words)
        if exp == "ABSTAIN":
            if abst: tn += 1; verdict = "TN"
            else: fp += 1; verdict = "FP"
        else:
            if abst: fn += 1; verdict = "FN"
            elif ok_words: tp += 1; verdict = "TP"
            else: fp += 1; wrong += 1; verdict = "FP(wrong answer)"
        detail.append((verdict, q, ans[:90].replace("\n", " ")))
    prec = tp / (tp + fp) if tp + fp else None
    rec = tp / (tp + fn) if tp + fn else None
    return {"qa_n": len(rows), "qa_tp": tp, "qa_fp": fp, "qa_fn": fn, "qa_tn": tn,
            "qa_wrong_answers": wrong, "qa_precision": prec, "qa_recall": rec,
            "lat_p50_ms": statistics.median(lat) if lat else None,
            "lat_p95_ms": sorted(lat)[max(0, int(round(0.95 * len(lat))) - 1)] if lat else None,
            "qa_detail": detail}


SHELL_TOOLS = [{"type": "function", "function": {"name": "bash", "description": "run shell",
               "parameters": {"type": "object", "properties": {"command": {"type": "string"}}}}}]
SHELL_CMDS = ["uname", "pwd", "whoami", "date", "ls", "hostname"]
SHELL_NONCMDS = ["frobnicatezz", "qwertyzz"]  # do not exist: re-probed on purpose (c23b306)
SHELL_SET = SHELL_CMDS + SHELL_NONCMDS


def shell_call(port, msgs):
    data, _ = post(port, {"model": "symbols", "tools": SHELL_TOOLS, "messages": msgs})
    tc = (data.get("choices") or [{}])[0].get("message", {}).get("tool_calls") or []
    return tc[0] if tc else None


def m_memory(port):
    """First pass: how many commands need a probe. The probe really runs and its
    output goes back to the server. Second pass: how many still need one."""
    if os.name == "nt":
        return {"mem_note": "not measured on Windows (the set uses bash)"}
    probes1 = probes2 = 0
    for cmd in SHELL_SET:
        tc = shell_call(port, [{"role": "user", "content": cmd}])
        if not tc:
            continue
        args = tc["function"]["arguments"]
        if "symbols-probe" in args and cmd in SHELL_CMDS:
            probes1 += 1
        shcmd = json.loads(args).get("command", "")
        out = subprocess.run(["bash", "-c", shcmd], capture_output=True, text=True).stdout
        post(port, {"model": "symbols", "tools": SHELL_TOOLS, "messages": [
            {"role": "user", "content": cmd},
            {"role": "assistant", "content": "", "tool_calls": [tc]},
            {"role": "tool", "tool_call_id": tc["id"], "content": out}]})
    for cmd in SHELL_SET:
        tc = shell_call(port, [{"role": "user", "content": cmd}])
        if tc and "symbols-probe" in tc["function"]["arguments"] and cmd in SHELL_CMDS:
            probes2 += 1
    return {"mem_probes_first": probes1, "mem_probes_repeat": probes2, "mem_n": len(SHELL_CMDS)}


# 4. C edit operator (OpenCode path) -----------------------------------------
def m_edit_ops():
    exe = BUILD / ("test_c_edit_ops" + EXE)
    if not exe.is_file():
        return {}
    _, out = run([str(exe)])
    m = re.search(r"TEST RESULTS: (\d+) passed, (\d+) failed", out)
    return {"edit_asserts_passed": int(m.group(1)) if m else None,
            "edit_asserts_failed": int(m.group(2)) if m else None}


# 5. Declared rules (checked inventory of what is still hand-written) --------
def m_rules():
    rows, stale = [], []
    for line in (ROOT / "tools/metrics/declared_rules.tsv").read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        name, where, symbol, kind = (line.split("\t") + ["", "", "", ""])[:4]
        text = (ROOT / where).read_text(encoding="utf-8", errors="replace") if (ROOT / where).is_file() else ""
        if symbol not in text:
            stale.append(name)
        rows.append((name, where, kind))
    tables = {}
    for t in ("data/agentic/tools.tsv", "data/agentic/fixtures.tsv", "data/english-spanish.txt"):
        p = ROOT / t
        if p.is_file():
            tables[t] = sum(1 for l in p.read_text(encoding="utf-8", errors="replace").splitlines()
                            if l.strip() and not l.startswith("#") and not l.startswith("TYPE"))
    return {"rules": rows, "rules_stale": stale, "tables": tables}


# 5b. Engineering task bank -------------------------------------------------------
def m_bank():
    """Engineering bank through tools/bank_harness.py (agent run + self-test)."""
    h = ROOT / "tools" / "bank_harness.py"
    if not (ROOT / "tests" / "fixtures" / "engineering_bank" / "index.tsv").is_file():
        return {"bank_note": "not measured (bank missing)"}
    agent = BUILD / ("symbols-agent.exe" if os.name == "nt" else "symbols-agent")
    if not agent.is_file():
        return {"bank_note": "not measured (`symbols-agent` not built)"}
    rc, out = run([sys.executable, str(h), "--agent", f"{agent} -w {{workdir}} {{task}}"], timeout=1800)
    try:
        res = json.loads(next(l for l in out.splitlines() if l.startswith("{")))
    except (ValueError, StopIteration):
        return {"bank_note": "not measured (harness output unreadable)"}
    rc2, out2 = run([sys.executable, str(h), "--self-test"], timeout=1800)
    try:
        st = json.loads(next(l for l in out2.splitlines() if l.startswith("{")))
        res["self_test"] = f"self-test {st['tasks_passed']}/{st['tasks_total']}"
    except (ValueError, StopIteration, KeyError):
        res["self_test"] = "self-test unreadable"
    return res


# 6. GitHub CI for the commit -----------------------------------------------------
def m_ci(sha):
    try:
        with urllib.request.urlopen(f"https://api.github.com/repos/{REPO}/actions/runs?head_sha={sha}", timeout=20) as f:
            runs = json.loads(f.read())["workflow_runs"]
        ci = [r for r in runs if r["name"] == "CI"]
        if not ci:  # apply-patch commits: the run that tests them carries the parent SHA
            parent = git("rev-parse", sha + "^")
            with urllib.request.urlopen(f"https://api.github.com/repos/{REPO}/actions/runs?head_sha={parent}", timeout=20) as f:
                ci = [r for r in json.loads(f.read())["workflow_runs"] if r["name"] == "Apply validated patch"]
        if not ci:
            return {"ci": "no CI run for this commit"}
        r = ci[0]
        with urllib.request.urlopen(r["jobs_url"], timeout=20) as f:
            jobs = json.loads(f.read())["jobs"]
        return {"ci_url": r["html_url"], "ci_jobs": {j["name"]: j["conclusion"] or j["status"] for j in jobs}}
    except Exception as e:  # no network or API rate limit
        return {"ci": f"not measured ({type(e).__name__})"}


def pct(x):
    return "not measured" if x is None else f"{100.0 * x:.0f}%"


def render(m):
    sha, date = m["sha"][:7], m["date"]
    L = [f"Measured on commit `{sha}` on {date} ({m['platform']}) with `python3 tools/metrics.py`.", "",
         "| Metric | Value | How it is measured |", "|---|---|---|"]
    c = m["ctest"]
    L.append(f"| CTest suite | {c['ctest_passed']}/{c['ctest_total']} pass, {c['ctest_failed']} fail, {c['ctest_skipped']} skipped | full `ctest`; skipped = optional data missing |")
    L.append(f"| Unit asserts | {c['asserts_passed']} pass, {c['asserts_failed']} fail | sum of `TEST RESULTS` over all tests |")
    r = m["runner"]
    for key, label in (("evaluation", "Agent: C repair, evaluation"), ("development", "Agent: C repair, development"), ("heldout", "Agent: C repair, held-out")):
        if key in r:
            x = r[key]
            ab = f", {x['correct_abstentions']} correct abstentions" if x.get("correct_abstentions") else ""
            L.append(f"| {label} | {x['resolved']}/{x['cases']} resolved ({pct(float(x['resolution_rate']))}), {x['false_positives']} harmful edits{ab} | `test_agent_runner_external` / `_heldout` (separated fixtures) |")
        else:
            L.append(f"| {label} | not measured | |")
    q = m.get("qa")
    if q and "qa_n" in q:
        L.append(f"| Grounded QA (fixed set of {q['qa_n']}) | precision {pct(q['qa_precision'])}, recall {pct(q['qa_recall'])} (TP {q['qa_tp']}, FP {q['qa_fp']}, FN {q['qa_fn']}, TN {q['qa_tn']}) | `tools/metrics/qa_battery.tsv` against the server; labels from the corpus, not from the engine |")
        L.append(f"| Invented or wrong answers | {q['qa_fp']} of {q['qa_n']} | FP from the row above |")
        L.append(f"| Server latency | p50 {q['lat_p50_ms']:.1f} ms, p95 {q['lat_p95_ms']:.1f} ms | same set, local |")
    else:
        L.append("| Grounded QA | not measured | `symbols-server` missing |")
    mm = m.get("memory", {})
    if "mem_probes_first" in mm:
        L.append(f"| Procedural memory: repeat benefit | {mm['mem_n']} real commands: {mm['mem_probes_first']} probes the first time → {mm['mem_probes_repeat']} on repeat | same list twice, the probe really runs; non-commands are re-probed on purpose |")
    else:
        L.append(f"| Procedural memory | {mm.get('mem_note', 'not measured')} | |")
    e = m.get("edit", {})
    if e:
        L.append(f"| C edit operator (OpenCode) | {e['edit_asserts_passed']} asserts pass, {e['edit_asserts_failed']} fail | `test_c_edit_ops` |")
    b = m.get("bank")
    if b and "tasks_total" in b:
        cats = ", ".join(f"{k} {v}" for k, v in b["passed_by_category"].items())
        L.append(f"| Engineering task bank ({b['tasks_total']} tasks, {len(b['passed_by_category'])} categories) | "
                 f"`symbols-agent`: {b['tasks_passed']}/{b['tasks_total']} pass ({pct(b['pass_rate'])}), "
                 f"{b['wrong_edits']} wrong edits, {b['untouched']} untouched; by category: {cats} | "
                 f"`tools/bank_harness.py` (before/ + task.md + check.py; golden after/ only in `--self-test`, "
                 f"{b.get('self_test', 'not run')}) |")
    else:
        L.append(f"| Engineering task bank | {(b or {}).get('bank_note', 'not measured')} | `tools/bank_harness.py` |")
    L.append("| Wikidata QA evaluation (`test_eval_*`) | not measured | `wiki_model.bin` missing (not bundled) |")
    ci = m.get("ci", {})
    if "ci_jobs" in ci:
        jobs = ", ".join(f"{k}: {v}" for k, v in sorted(ci["ci_jobs"].items()))
        L.append(f"| CI per platform | {jobs} | [run]({ci['ci_url']}); failing tests are listed in each job log |")
    else:
        L.append(f"| CI per platform | {ci.get('ci', 'not measured')} | |")
    rl = m["rules"]
    L.append(f"| Hand-written rules (declared) | {len(rl['rules'])} rules; tables: " +
             ", ".join(f"`{Path(k).name}` {v} rows" for k, v in rl["tables"].items()) +
             (f"; **stale: {', '.join(rl['rules_stale'])}**" if rl["rules_stale"] else "") +
             " | `tools/metrics/declared_rules.tsv` (the script checks each one is still in the code) |")
    return "\n".join(L)


def main():
    write = "--write" in sys.argv
    sha = git("rev-parse", "HEAD")
    m = {"sha": sha, "date": datetime.now(timezone.utc).strftime("%Y-%m-%d"),
         "platform": f"{sys.platform}, {BUILD.name}"}
    m["ctest"] = m_ctest()
    m["runner"] = m_runner()
    m["edit"] = m_edit_ops()
    tmp = tempfile.mkdtemp()
    srv, port = start_server(os.path.join(tmp, "proc.tsv"))
    if srv:
        try:
            m["qa"] = m_qa(port)
            m["memory"] = m_memory(port)
        finally:
            srv.kill(); srv.wait()
    shutil.rmtree(tmp, ignore_errors=True)
    m["bank"] = m_bank()
    m["rules"] = m_rules()
    if "--ci" in sys.argv:
        m["ci"] = m_ci(sha)
    table = render(m)
    print(table)
    if m.get("qa"):
        print("\nQA detail:")
        for v, qq, a in m["qa"]["qa_detail"]:
            print(f"  {v:24} {qq}  =>  {a}")
    if write:
        readme = ROOT / "README.md"
        s = readme.read_text(encoding="utf-8")
        b, e = "<!-- METRICS:BEGIN -->", "<!-- METRICS:END -->"
        if b in s and e in s:
            s = s[:s.index(b) + len(b)] + "\n" + table + "\n" + s[s.index(e):]
            readme.write_text(s, encoding="utf-8")
        hist = ROOT / "tools/metrics_history.csv"
        new = not hist.is_file()
        with hist.open("a", newline="", encoding="utf-8") as f:
            w = csv.writer(f, lineterminator="\n")
            if new:
                w.writerow(["date", "commit", "ctest_pass", "ctest_total", "asserts_pass", "eval_resolved", "eval_cases",
                            "eval_fp", "qa_tp", "qa_fp", "qa_fn", "qa_tn", "lat_p50_ms", "mem_probes_first", "mem_probes_repeat"])
            ev = m["runner"].get("evaluation", {}); q = m.get("qa", {}); mm = m.get("memory", {})
            w.writerow([m["date"], sha[:7], m["ctest"]["ctest_passed"], m["ctest"]["ctest_total"], m["ctest"]["asserts_passed"],
                        ev.get("resolved"), ev.get("cases"), ev.get("false_positives"), q.get("qa_tp"), q.get("qa_fp"),
                        q.get("qa_fn"), q.get("qa_tn"), round(q["lat_p50_ms"]) if q.get("lat_p50_ms") else None,
                        mm.get("mem_probes_first"), mm.get("mem_probes_repeat")])


if __name__ == "__main__":
    main()
