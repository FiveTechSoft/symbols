#!/usr/bin/env python3
"""Classify CTest results (M3 slice, S3).

    ctest_report.py TEST_XML [--registered BUILD_DIR]

TEST_XML is the Test.xml that `ctest -T Test` writes under BUILD/Testing/<stamp>/.
It prints one JSON object: a record per test (name, class, detail, exit_code,
exit_value, seconds) and a count per class. The classes are passed, failed,
timed_out, crashed, skipped, not_run and unknown.

Why not JUnit: `ctest --output-junit` writes a timeout, an abort and a segfault
all as <failure message="">, so it cannot tell them apart. Test.xml carries the
CTest reason in the "Exit Code" and "Completion Status" measurements.

Fail-closed: only the reason strings read from CTest 3.22.1 (listed below) are
classified. Any other string, a missing field or an unknown Status gives
"unknown", never "passed". A malformed or empty file exits 2 with no report.
With --registered, a test the build registers but Test.xml lacks is reported as
not_run with detail "absent from Test.xml" (for example after `ctest -R`).
Not measured: Windows reasons (CTest writes other strings there)."""
import json, os, subprocess, sys
import xml.etree.ElementTree as ET

CLASSES = ["passed", "failed", "timed_out", "crashed", "skipped", "not_run", "unknown"]
# "Exit Code" values of a Status="failed" test, read from CTest 3.22.1.
FAILED_REASONS = {"Failed": "failed", "Timeout": "timed_out"}
CRASH_REASONS = ["SEGFAULT", "Subprocess aborted", "NUMERICAL", "ILLEGAL", "Subprocess killed", "Subprocess terminated"]
# "Completion Status" values of a Status="notrun" test.
SKIP_REASONS = ["SKIP_RETURN_CODE=77", "SKIP_REGULAR_EXPRESSION_MATCHED"]
NOTRUN_REASONS = ["Unable to find executable", "Disabled"]


def classify(status, exit_code, completion):
    """Return (class, detail). exit_code and completion are None when absent."""
    if status == "passed":
        if completion == "Completed" and exit_code in (None, "Completed"):
            return "passed", ""
        return "unknown", "passed with Exit Code %r, Completion Status %r" % (exit_code, completion)
    if status == "failed":
        if completion != "Completed":
            return "unknown", "failed with Completion Status %r" % completion
        if exit_code in FAILED_REASONS:
            return FAILED_REASONS[exit_code], exit_code
        if exit_code in CRASH_REASONS:
            return "crashed", exit_code
        return "unknown", "failed with Exit Code %r" % exit_code
    if status == "notrun":
        if completion in SKIP_REASONS:
            return "skipped", completion
        if completion in NOTRUN_REASONS:
            return "not_run", completion
        return "unknown", "notrun with Completion Status %r" % completion
    return "unknown", "Status %r" % status


def parse(path):
    root = ET.parse(path).getroot()
    out = []
    for t in root.iter("Test"):
        status = t.get("Status")
        name = t.findtext("Name")
        if len(t) == 0:
            continue            # a <TestList> entry is a bare <Test>name</Test>
        meas = {}
        for n in t.iter("NamedMeasurement"):
            if n.get("name") is not None:
                meas[n.get("name")] = n.findtext("Value") or ""
        cls, detail = classify(status, meas.get("Exit Code"), meas.get("Completion Status"))
        if not name:
            cls, detail, name = "unknown", "a test with no Name", ""
        rec = {"name": name, "class": cls, "detail": detail,
               "exit_code": meas.get("Exit Code"), "exit_value": meas.get("Exit Value"),
               "seconds": meas.get("Execution Time")}
        out.append(rec)
    return out


def registered(build):
    p = subprocess.run([os.environ.get("CTEST_COMMAND", "ctest"), "--test-dir", build, "--show-only=json-v1"], capture_output=True, text=True, timeout=60)
    if p.returncode != 0:
        raise ValueError("ctest --show-only failed")
    return [t["name"] for t in json.loads(p.stdout)["tests"]]


def main(argv):
    if len(argv) not in (2, 4) or (len(argv) == 4 and argv[2] != "--registered"):
        print("usage: ctest_report.py TEST_XML [--registered BUILD_DIR]", file=sys.stderr)
        return 2
    try:
        recs = parse(argv[1])
        if not recs:
            raise ValueError("no test with a Status in the file")
        if len(argv) == 4:
            seen = set(r["name"] for r in recs)
            for n in registered(argv[3]):
                if n not in seen:
                    recs.append({"name": n, "class": "not_run", "detail": "absent from Test.xml",
                                 "exit_code": None, "exit_value": None, "seconds": None})
    except (ET.ParseError, OSError, ValueError, KeyError) as e:
        print("ctest_report: %s" % e, file=sys.stderr)
        return 2
    counts = {c: 0 for c in CLASSES}
    for r in recs:
        counts[r["class"]] += 1
    recs.sort(key=lambda r: r["name"])
    print(json.dumps({"counts": counts, "tests": recs}, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
