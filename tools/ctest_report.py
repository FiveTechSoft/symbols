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
Structural ambiguity (wrong root/Testing layout, duplicate test identities, Name
fields or critical measurements, including equal duplicates) exits 2 without JSON.
Registered names must be nonempty unique strings and cover the XML result names.
Unknown well-formed reason/status strings still produce unknown.
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


CRITICAL_MEASUREMENTS = {"Exit Code", "Completion Status", "Exit Value", "Execution Time"}


def validate_names(names, context):
    if any(not isinstance(n, str) or not n.strip() for n in names):
        raise ValueError("%s has an empty or invalid name" % context)
    if len(names) != len(set(names)):
        raise ValueError("%s has duplicate names" % context)


def validate_structure(root):
    """Validate real results before parsing; bare TestList entries are metadata."""
    if root.tag != "Site":
        raise ValueError("root is not Site")
    sections = root.findall("Testing")
    if len(sections) != 1 or len(list(root.iter("Testing"))) != 1:
        raise ValueError("expected one direct Testing section")
    section = sections[0]
    real = section.findall("Test")
    lists = section.findall("TestList")
    listed = [t for group in lists for t in group.findall("Test")]
    if len(real) + len(listed) != len(list(root.iter("Test"))) or any(len(t) or t.attrib for t in listed):
        raise ValueError("Test outside result or bare TestList entry")
    for t in real:
        names = t.findall("Name")
        if len(names) != 1 or len(names[0]):
            raise ValueError("test needs exactly one scalar Name")
        validate_names([t.findtext("Name")], "test")
        critical = set()
        for n in t.iter("NamedMeasurement"):
            key = n.get("name")
            if key in CRITICAL_MEASUREMENTS:
                if key in critical:
                    raise ValueError("duplicate critical measurement: %s" % key)
                critical.add(key)
                if len(n.findall("Value")) > 1:
                    raise ValueError("duplicate critical measurement Value: %s" % key)
    validate_names([t.findtext("Name") for t in real], "Test.xml")


def parse(path):
    root = ET.parse(path).getroot()
    validate_structure(root)
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
    names = [t["name"] for t in json.loads(p.stdout)["tests"]]
    validate_names(names, "registered tests")
    return names


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
            names = registered(argv[3])
            if not (seen - {""}) <= set(names):
                raise ValueError("Test.xml names are absent from registered tests")
            for n in names:
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
