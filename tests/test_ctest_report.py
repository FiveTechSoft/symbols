#!/usr/bin/env python3
"""S3: tools/ctest_report.py classifies CTest results. Run: argv[1] cmake, argv[2] ctest.
A labeled fixture (tests/fixtures/ctest_report, 17 tests, one reason each) is built
and run with `ctest -T Test`; the tool must give each test its designed class:
  passed        pass, will_fail (WILL_FAIL inverts the exit code)
  failed        fail, fail_regex, regex_fail, exit77_noskip (77 without SKIP_RETURN_CODE)
  timed_out     timeout
  crashed       segv, abort, fpe, ill, kill9, term
  skipped       skip (SKIP_RETURN_CODE), skip_regex (SKIP_REGULAR_EXPRESSION)
  not_run       notrun (executable absent), notrun_disabled
Further checks: no phantom tests from <TestList>; counts add up; edited copies of the
real Test.xml (an unknown Exit Code, Status, or Completion Status) give "unknown" and
never "passed"; a malformed, missing or empty file exits 2 with no report; with
--registered, tests missing after `ctest -R` are not_run "absent from Test.xml".
Then 10 mutants of the tool (text replacements in a temp copy) are run against the same
checks; each must fail exactly its expected set of checks, and the test exits 0 only if
the baseline passes and every mutant fails exactly that set (no WILL_FAIL).
Not covered: Windows (SKIP: its reason strings differ and are unmeasured), CTest other
than the version CI uses, and reasons not in the fixture (they land in "unknown")."""
import json, os, re, shutil, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.dirname(HERE)
TOOL = os.path.join(SRC, "tools", "ctest_report.py")
EXPECT = {"pass": "passed", "will_fail": "passed", "fail": "failed", "fail_regex": "failed", "regex_fail": "failed",
          "exit77_noskip": "failed", "timeout": "timed_out", "segv": "crashed", "abort": "crashed", "fpe": "crashed",
          "ill": "crashed", "kill9": "crashed", "term": "crashed", "skip": "skipped", "skip_regex": "skipped",
          "notrun": "not_run", "notrun_disabled": "not_run"}


def run_tool(tool, args, env):
    p = subprocess.run([sys.executable, tool] + args, capture_output=True, text=True, env=env, timeout=60)
    return p.returncode, p.stdout, p.stderr


def checks(tool, xml, xml_r, build, tmp, env):
    """Return the set of failed check names for this copy of the tool."""
    bad = set()

    def chk(name, ok):
        if not ok:
            bad.add(name)

    def report(path, extra=()):
        rc, out, err = run_tool(tool, [path] + list(extra), env)
        try:
            return rc, json.loads(out)
        except ValueError:
            return rc, None

    rc, d = report(xml)
    chk("baseline_runs", rc == 0 and d is not None)
    got = {t["name"]: t["class"] for t in d["tests"]} if d else {}
    for n, c in EXPECT.items():
        chk("class:" + n, got.get(n) == c)
    chk("no_phantom_tests", d is not None and len(d["tests"]) == len(EXPECT))
    chk("counts_add_up", d is not None and sum(d["counts"].values()) == len(d["tests"]) and
        d["counts"]["unknown"] == 0)
    text = open(xml).read()

    def variant(name, old, new, test, want):
        t = text.replace(old, new, 1) if old in text else None
        if t is None or t == text:
            chk(name, False)
            return
        p = os.path.join(tmp, name + ".xml")
        open(p, "w").write(t)
        rc, d2 = report(p)
        cls = {t["name"]: t["class"] for t in d2["tests"]}.get(test) if d2 else None
        chk(name, rc == 0 and cls == want)
    # the first Exit Code "Failed" belongs to the test named in the block: edit inside that block
    def block_edit(name, test, old, new, want):
        m = re.search(r"<Test Status=\"\w+\">\s*<Name>%s</Name>.*?</Test>" % re.escape(test), text, re.S)
        if not m or old not in m.group(0):
            chk(name, False)
            return
        t = text.replace(m.group(0), m.group(0).replace(old, new, 1), 1)
        p = os.path.join(tmp, name + ".xml")
        open(p, "w").write(t)
        rc, d2 = report(p)
        cls = {x["name"]: x["class"] for x in d2["tests"]}.get(test) if d2 else None
        chk(name, rc == 0 and cls == want)
    block_edit("unknown_exit_code", "fail", "<Value>Failed</Value>", "<Value>Bogus</Value>", "unknown")
    block_edit("unknown_status", "fail", 'Status="failed"', 'Status="weird"', "unknown")
    block_edit("passed_bad_completion", "pass", "<Value>Completed</Value>", "<Value>Bogus</Value>", "unknown")
    block_edit("notrun_bad_completion", "notrun", "<Value>Unable to find executable</Value>", "<Value>Bogus</Value>", "unknown")
    block_edit("failed_bad_completion", "fail", "<Value>Completed</Value>", "<Value>Bogus</Value>", "unknown")
    # fail-closed inputs
    for name, content in (("malformed", "not xml"), ("empty_testing", "<Site><Testing></Testing></Site>")):
        p = os.path.join(tmp, name + ".xml")
        open(p, "w").write(content)
        rc, out, err = run_tool(tool, [p], env)
        chk("fail_closed_" + name, rc == 2 and out == "")
    rc, out, err = run_tool(tool, [os.path.join(tmp, "missing.xml")], env)
    chk("fail_closed_missing", rc == 2 and out == "")
    rc, d3 = report(xml_r, ["--registered", build])
    absent = {t["name"] for t in d3["tests"] if t["detail"] == "absent from Test.xml" and t["class"] == "not_run"} if d3 else set()
    chk("registered_absent", rc == 0 and d3 is not None and absent == set(EXPECT) - {"pass", "fail"})
    return bad


MUTANTS = [
    ("timeout_as_failed", 'FAILED_REASONS = {"Failed": "failed", "Timeout": "timed_out"}',
     'FAILED_REASONS = {"Failed": "failed", "Timeout": "failed"}', {"class:timeout"}),
    ("segv_unlisted", '"SEGFAULT", ', "", {"class:segv", "counts_add_up"}),
    ("crash_as_failed", 'return "crashed", exit_code', 'return "failed", exit_code',
     {"class:segv", "class:abort", "class:fpe", "class:ill", "class:kill9", "class:term"}),
    ("skip_regex_unlisted", ', "SKIP_REGULAR_EXPRESSION_MATCHED"]', "]", {"class:skip_regex", "counts_add_up"}),
    ("skip_as_not_run", 'return "skipped", completion', 'return "not_run", completion', {"class:skip", "class:skip_regex"}),
    ("unknown_exit_passes", 'return "unknown", "failed with Exit Code %r" % exit_code',
     'return "passed", "failed with Exit Code %r" % exit_code', {"unknown_exit_code"}),
    ("unknown_status_passes", 'return "unknown", "Status %r" % status', 'return "passed", "Status %r" % status',
     {"unknown_status"}),
    ("passed_ignores_completion", 'if completion == "Completed" and exit_code in (None, "Completed"):',
     'if exit_code in (None, "Completed"):', {"passed_bad_completion"}),
    ("empty_accepted", "        if not recs:\n            raise ValueError", "        if False:\n            raise ValueError",
     {"fail_closed_empty_testing"}),
    ("testlist_counted", "        if len(t) == 0:", "        if False:", {"no_phantom_tests", "counts_add_up"}),
]


def main():
    if os.name == "nt":
        print("SKIP: CTest reason strings are not measured on Windows")
        return 77
    cmake, ctest = sys.argv[1], sys.argv[2]
    tmp = tempfile.mkdtemp()
    env = dict(os.environ, CTEST_COMMAND=ctest)
    fails = []
    try:
        build = os.path.join(tmp, "b")
        fx = os.path.join(SRC, "tests", "fixtures", "ctest_report")
        for cmd in ([cmake, "-S", fx, "-B", build], [cmake, "--build", build]):
            p = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
            if p.returncode != 0:
                print("FAIL fixture build", p.stderr[-300:])
                return 1
        subprocess.run([ctest, "--test-dir", build, "-T", "Test", "-R", "^(pass|fail)$"], capture_output=True, text=True, timeout=120)
        xml_r = [os.path.join(dp, f) for dp, _, fs in os.walk(os.path.join(build, "Testing")) for f in fs if f == "Test.xml"][0]
        keep = os.path.join(tmp, "Test_r.xml")
        shutil.copy(xml_r, keep)
        shutil.rmtree(os.path.join(build, "Testing"))
        subprocess.run([ctest, "--test-dir", build, "-T", "Test"], capture_output=True, text=True, timeout=120)
        xml = [os.path.join(dp, f) for dp, _, fs in os.walk(os.path.join(build, "Testing")) for f in fs if f == "Test.xml"][0]
        base = checks(TOOL, xml, keep, build, tmp, env)
        if base:
            fails.append("baseline: %s" % sorted(base))
        for name, old, new, want in MUTANTS:
            s = open(TOOL).read()
            if s.count(old) != 1:
                fails.append("mutant %s: pattern found %d times" % (name, s.count(old)))
                continue
            mt = os.path.join(tmp, "mut_" + name + ".py")
            open(mt, "w").write(s.replace(old, new))
            got = checks(mt, xml, keep, build, tmp, env)
            if got != want:
                fails.append("mutant %s: failed %s, expected %s" % (name, sorted(got), sorted(want)))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    if fails:
        for f in fails:
            print("FAIL", f)
        return 1
    print("PASS ctest_report: %d tests classified, %d mutants killed with their exact sets" % (len(EXPECT), len(MUTANTS)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
