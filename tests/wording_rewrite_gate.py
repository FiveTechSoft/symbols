#!/usr/bin/env python3
"""Require successful noop baseline execution and its whole counts-only summary.

This does not prove a rewrite was consumed: noop ignores task text.
"""
import argparse
import collections
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile


class GateError(ValueError):
    pass


def metadata(path):
    categories = collections.Counter()
    splits = collections.Counter({"dev": 0, "heldout": 0})
    ids = set()
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        parts = line.split("\t")
        if not line.strip() or parts[0].startswith("#") or parts[0] == "id":
            continue
        if len(parts) < 3 or not all(parts[:3]) or parts[0] in ids:
            raise GateError("invalid/duplicate bank metadata")
        ids.add(parts[0])
        categories[parts[1]] += 1
        suffix = parts[0].rsplit("_", 1)[-1]
        splits["dev" if suffix.isdigit() and int(suffix) % 2 else "heldout"] += 1
    if not ids:
        raise GateError("empty bank")
    return dict(sorted(categories.items())), dict(splits)


def expected(categories, splits):
    total = sum(categories.values())
    return {
        "schema": 1, "mode": "agent", "agent": "noop",
        "tasks_total": total, "tasks_passed": 0, "pass_rate": 0.0,
        "pass_rate_by_category": {k: 0.0 for k in categories},
        "passed_by_category": {k: f"0/{v}" for k, v in categories.items()},
        "wrong_edits": 0, "untouched": total, "agent_errors": 0,
        "setup_failures": 0, "wall_ms_total": 0.0,
        "passed_by_split": {k: f"0/{v}" for k, v in splits.items()},
    }


def summary(value):
    keys = ("mode", "tasks_total", "tasks_passed", "pass_rate", "wrong_edits",
            "untouched", "agent_errors", "setup_failures")
    # summarize() emits a float for nonempty banks; preserve its spelling here.
    fields = [f"{k}={value[k]}" for k in keys]
    fields.extend(f"pass_{k}={value['passed_by_split'][k]}" for k in ("dev", "heldout"))
    return "BANK_HARNESS " + " ".join(fields)


def pairs_no_duplicates(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise GateError("duplicate JSON key")
        result[key] = value
    return result


def reject_constant(value):
    raise GateError("nonfinite JSON constant " + value)


def validate(stdout, stderr, code, categories, splits):
    if code != 0:
        raise GateError("harness exit must be zero")
    try:
        value = json.loads(stdout, object_pairs_hook=pairs_no_duplicates,
                           parse_constant=reject_constant)
    except (ValueError, TypeError) as exc:
        raise GateError("invalid JSON: " + str(exc)) from exc
    want = expected(categories, splits)
    if type(value) is not dict or set(value) != set(want):
        raise GateError("wrong top-level fields")
    for key, val in want.items():
        if key == "wall_ms_total":
            if type(value[key]) not in (int, float) or not math.isfinite(value[key]) or value[key] < 0:
                raise GateError("invalid wall time")
        elif key in ("pass_rate",):
            if type(value[key]) not in (int, float) or value[key] != val:
                raise GateError("wrong " + key)
        elif type(val) is int:
            if type(value[key]) is not int or value[key] != val:
                raise GateError("wrong " + key)
        elif type(val) is dict:
            actual = value[key]
            if type(actual) is not dict or set(actual) != set(val):
                raise GateError("wrong " + key)
            for name, expected_value in val.items():
                if type(expected_value) is float:
                    if type(actual[name]) not in (int, float) or actual[name] != expected_value:
                        raise GateError("wrong category rate")
                elif type(actual[name]) is not str or actual[name] != expected_value:
                    raise GateError("wrong category/split count")
        elif type(value[key]) is not type(val) or value[key] != val:
            raise GateError("wrong " + key)
    if stderr not in (summary(want), summary(want) + "\n"):
        raise GateError("stderr summary mismatch")
    return value


def run(command, cwd, categories, splits, timeout=100):
    try:
        result = subprocess.run(command, cwd=cwd, capture_output=True, text=True,
                                encoding="utf-8", errors="strict", timeout=timeout,
                                shell=False)
    except (OSError, UnicodeError, subprocess.TimeoutExpired) as exc:
        raise GateError("harness execution failed: " + type(exc).__name__) from exc
    return validate(result.stdout, result.stderr, result.returncode, categories, splits)


def self_test():
    categories = {"alpha": 3, "beta": 2}
    splits = {"dev": 3, "heldout": 2}
    good = expected(categories, splits)
    text, err = json.dumps(good), summary(good) + "\n"
    validate(text, err, 0, categories, splits)
    count = 0

    def refused(out=text, stderr=err, code=0, cats=categories):
        nonlocal count
        try:
            validate(out, stderr, code, cats, splits)
        except GateError:
            count += 1
            return
        raise AssertionError("negative accepted")

    refused(code=1)
    refused(out='{"tasks_passed": 0}')
    refused(out='{"tasks_passed": 0}', code=1)
    refused(out="", stderr='{"tasks_passed": 0}\n')
    changes = [
        ("mode", "self-test"), ("agent", "golden after/"), ("schema", 2),
        ("schema", True), ("tasks_total", 0), ("tasks_total", 4),
        ("pass_rate_by_category", {"alpha": 0.0}),
        ("passed_by_split", {"dev": "0/2", "heldout": "0/3"}),
        ("setup_failures", 1), ("agent_errors", 1), ("wrong_edits", 1),
        ("untouched", 4), ("tasks_passed", 1),
        ("wall_ms_total", float("nan")), ("wall_ms_total", float("inf")),
        ("wall_ms_total", -1), ("wall_ms_total", True),
    ]
    for key, value in changes:
        bad = dict(good)
        bad[key] = value
        refused(out=json.dumps(bad))
    refused(out=text[:-1] + ', "tasks_passed": 0}')
    bad = dict(good)
    bad["tasks"] = []
    refused(out=json.dumps(bad))
    refused(out=text + "\nextra")
    refused(out="not JSON")
    refused(out=text[:-1])
    refused(stderr=err.replace("untouched=5", "untouched=4"))
    refused(stderr=err + "extra diagnostic\n")
    refused(cats={"alpha": 4, "beta": 2})
    bad = dict(good)
    bad["tasks_total"] = 4
    refused(out=json.dumps(bad), stderr=summary(bad) + "\n")
    coherent = expected({"alpha": 4, "beta": 2}, splits)
    refused(out=json.dumps(coherent), stderr=summary(coherent) + "\n")
    with tempfile.TemporaryDirectory(prefix="wording_gate_") as td:
        file = Path(td) / "fake.py"
        control = "import sys\nprint(" + repr(text) + ")\nprint(" + repr(err.rstrip()) + ", file=sys.stderr)\n"
        file.write_text(control, encoding="utf-8")
        run([sys.executable, str(file)], td, categories, splits)
        file.write_text(control + "sys.exit(1)\n", encoding="utf-8")
        try:
            run([sys.executable, str(file)], td, categories, splits)
        except GateError:
            count += 1
        else:
            raise AssertionError("runner accepted marker plus exit1")
        file.write_text("import time\ntime.sleep(1)\n", encoding="utf-8")
        try:
            run([sys.executable, str(file)], td, categories, splits, timeout=0.02)
        except GateError:
            count += 1
        else:
            raise AssertionError("runner accepted timeout")
    print(f"WORDING_REWRITE_GATE selftest passed: negatives={count} controls=2")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    try:
        if args.self_test:
            return self_test()
        root = Path(__file__).resolve().parent.parent
        cats, splits = metadata(root / "tests/fixtures/engineering_bank/index.tsv")
        result = run([sys.executable, str(root / "tools/bank_harness.py"),
                      "--agent", "noop", "--rewrite", "R3", "--counts-only"],
                     root, cats, splits)
        print("WORDING_REWRITE_GATE passed: " + summary(result))
        return 0
    except (GateError, OSError, UnicodeError) as exc:
        print("WORDING_REWRITE_GATE refused: " + str(exc), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
