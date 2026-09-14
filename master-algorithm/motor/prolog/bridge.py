"""
Bridge: Python kernel ↔ Prolog archive/critic.

Prefer SWI-Prolog (`swipl`). If unavailable, fall back to the tiny Horn
interpreter in horn.py for the subset of checks we need (recurrence / bit_fn
/ period), so the motor never hard-depends on apt packages at runtime.
"""

from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import tempfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Optional

from .horn import HornKB

MOTOR_DIR = Path(__file__).resolve().parent.parent
PROLOG_DIR = Path(__file__).resolve().parent
CRITIC_PL = PROLOG_DIR / "critic.pl"
DEFAULT_ARCHIVE = MOTOR_DIR / "archive" / "theory.pl"
DEFAULT_META = MOTOR_DIR / "archive" / "meta.json"


def swipl_available() -> bool:
    return shutil.which("swipl") is not None


def _escape_atom(s: str) -> str:
    s = s.replace("\\", "\\\\").replace("'", "\\'")
    return f"'{s}'"


@dataclass
class PrologArchive:
    """
    Growing Horn theory on disk (theory.pl) + JSON meta for scheduler state.

    Prolog side = facts/lemmas/rejected as clauses.
    Python side = UCB family state, step counters, curve points.
    """

    theory_path: Path = field(default_factory=lambda: DEFAULT_ARCHIVE)
    meta_path: Path = field(default_factory=lambda: DEFAULT_META)
    backend: str = "swipl"  # or "horn"
    _lines: list[str] = field(default_factory=list)
    meta: dict[str, Any] = field(default_factory=dict)

    def __post_init__(self) -> None:
        self.theory_path.parent.mkdir(parents=True, exist_ok=True)
        if not swipl_available():
            self.backend = "horn"
        if self.theory_path.exists():
            self._lines = self.theory_path.read_text(encoding="utf-8").splitlines()
        else:
            self._lines = [
                "% Autodidactic motor — growing Horn theory",
                "% Appended by Python kernel; queried by critic (SWI or Horn fallback).",
                ":- dynamic obs/3, rec/2, rejected/2, verified/1, lemma/3.",
                ":- dynamic holds_bit/3, bit_fn/3, companion/2, true_mod/3.",
                "",
                "% Companion graph for sequence transfer",
                "companion(fib, lucas).",
                "companion(fib, pell).",
                "companion(lucas, fib).",
                "companion(lucas, pell).",
                "companion(pell, fib).",
                "companion(pell, lucas).",
                "",
            ]
            self._flush_theory()
        if self.meta_path.exists():
            self.meta = json.loads(self.meta_path.read_text(encoding="utf-8"))
        else:
            self.meta = {
                "total_steps": 0,
                "arms": {},  # "world::family" -> FamilySpec dict
                "step_log": [],
                "curves": [],
                "baselines": {},
                "backend": self.backend,
            }

    # --- persistence ---

    def _flush_theory(self) -> None:
        self.theory_path.write_text("\n".join(self._lines) + "\n", encoding="utf-8")

    def save_meta(self) -> None:
        self.meta["backend"] = self.backend
        self.meta_path.write_text(json.dumps(self.meta, indent=2), encoding="utf-8")

    def save(self) -> None:
        self._flush_theory()
        self.save_meta()
        try:
            from motor.snapshot import dump_quietly
            dump_quietly(self.theory_path.parent)
        except Exception:
            pass

    def theory_text(self) -> str:
        return "\n".join(self._lines) + "\n"

    def append_clause(self, clause: str, comment: Optional[str] = None) -> None:
        """Append a Horn clause (must end with period). Idempotent on exact line."""
        clause = clause.strip()
        if not clause.endswith("."):
            clause += "."
        if clause in self._lines:
            return
        if comment:
            self._lines.append(f"% {comment}")
        self._lines.append(clause)
        self._flush_theory()

    def assert_obs(self, seq: str, n: int, v: int) -> None:
        self.append_clause(f"obs({seq}, {n}, {v}).")

    def assert_rec(self, seq: str, coeffs: list[int]) -> None:
        cstr = "[" + ",".join(str(c) for c in coeffs) + "]"
        self.append_clause(f"rec({seq}, {cstr}).", comment=f"learned recurrence on {seq}")

    def assert_verified(self, world: str, family: str, name: str, formula: str) -> None:
        self.append_clause(
            f"verified(fact({world}, {family}, {_escape_atom(name)}, {_escape_atom(formula)})).",
            comment=f"verified @ {world}/{family}",
        )

    def assert_rejected(self, name: str, why: str) -> None:
        self.append_clause(
            f"rejected({_escape_atom(name)}, {_escape_atom(why)}).",
            comment="finite fail / dead-end",
        )

    def assert_lemma(self, name: str, kind: str, formula: str) -> None:
        self.append_clause(
            f"lemma({_escape_atom(name)}, {kind}, {_escape_atom(formula)}).",
        )

    def assert_bit_fn(self, name: str, kind: str, spec: str) -> None:
        self.append_clause(f"bit_fn({_escape_atom(name)}, {kind}, {spec}).")

    def assert_holds_bit(self, ex_id: int, bits: list[int], out: int) -> None:
        bl = "[" + ",".join(str(b) for b in bits) + "]"
        self.append_clause(f"holds_bit({ex_id}, {bl}, {out}).")

    def assert_mod_period(self, seq: str, m: int, period: int) -> None:
        self.append_clause(f"true_mod({seq}, {m}, {period}).")

    def count_verified(self) -> int:
        return sum(1 for ln in self._lines if ln.startswith("verified("))

    def count_rejected(self) -> int:
        return sum(1 for ln in self._lines if ln.startswith("rejected("))

    def count_lemmas(self) -> int:
        return sum(1 for ln in self._lines if ln.startswith("lemma("))

    def list_bit_fns(self) -> list[tuple[str, str, str]]:
        """Return [(name, kind/target, spec/hyp), ...] from bit_fn/3 clauses."""
        out = []
        for ln in self._lines:
            m = re.match(r"bit_fn\('([^']+)',\s*(\w+),\s*([^)]+)\)\.", ln)
            if m:
                spec = m.group(3).strip().strip("'")
                out.append((m.group(1), m.group(2), spec))
            else:
                m2 = re.match(r"bit_fn\(([^,]+),\s*(\w+),\s*([^)]+)\)\.", ln)
                if m2:
                    name = m2.group(1).strip().strip("'")
                    spec = m2.group(3).strip().strip("'")
                    out.append((name, m2.group(2), spec))
        return out

    def list_recs(self) -> list[tuple[str, list[int]]]:
        out = []
        for ln in self._lines:
            m = re.match(r"rec\((\w+),\s*\[([^\]]*)\]\)\.", ln)
            if m:
                coeffs = [int(x) for x in m.group(2).split(",") if x.strip()]
                out.append((m.group(1), coeffs))
        return out

    def verified_names(self) -> list[str]:
        names = []
        for ln in self._lines:
            m = re.search(r"verified\(fact\([^,]+,\s*[^,]+,\s*'([^']+)'", ln)
            if m:
                names.append(m.group(1))
            else:
                m2 = re.search(r"verified\(fact\([^,]+,\s*[^,]+,\s*([^,]+),", ln)
                if m2 and not m2.group(1).startswith("'"):
                    names.append(m2.group(1).strip())
        return names

    def distinct_types_approx(self) -> set[str]:
        """Relation types from verified/lemma/rec lines (coarse)."""
        types: set[str] = set()
        for ln in self._lines:
            if ln.startswith("rec("):
                types.add("linear_recurrence")
            if ln.startswith("lemma("):
                m = re.match(r"lemma\([^,]+,\s*(\w+)", ln)
                if m:
                    types.add(f"lemma:{m.group(1)}")
            if ln.startswith("bit_fn("):
                types.add("bit_fn")
            if ln.startswith("true_mod("):
                types.add("modular_period")
            if ln.startswith("verified("):
                # extract family atom (2nd arg)
                m = re.match(r"verified\(fact\([^,]+,\s*(\w+),", ln)
                if m:
                    types.add(m.group(1))
        return types


class PrologCritic:
    """
    verify = query/resolution against theory + critic.pl
    reject = finite fail (+ optional counterexample computed in Python)
    """

    def __init__(self, archive: PrologArchive) -> None:
        self.archive = archive
        self.backend = archive.backend

    def _swipl_query(self, goal: str, extra_assert: Optional[list[str]] = None, timeout: float = 10.0) -> bool:
        """Run goal against critic.pl + theory.pl (+ optional temp asserts). Return True if goal succeeds."""
        with tempfile.TemporaryDirectory() as td:
            td_path = Path(td)
            query_pl = td_path / "query.pl"
            parts = [
                f":- consult('{CRITIC_PL.as_posix()}').",
                f":- consult('{self.archive.theory_path.as_posix()}').",
            ]
            if extra_assert:
                for c in extra_assert:
                    c = c.strip()
                    if not c.endswith("."):
                        c += "."
                    parts.append(c)  # directive-less facts need assert — write as file facts
            # Write extra as a small facts file consulted after
            extra_file = td_path / "extra.pl"
            if extra_assert:
                extra_file.write_text(
                    "\n".join(
                        (c if c.strip().endswith(".") else c.strip() + ".")
                        for c in extra_assert
                    )
                    + "\n",
                    encoding="utf-8",
                )
                parts.append(f":- consult('{extra_file.as_posix()}').")
            parts.append(f":- ( {goal} -> halt(0) ; halt(1) ).")
            query_pl.write_text("\n".join(parts) + "\n", encoding="utf-8")
            try:
                r = subprocess.run(
                    ["swipl", "-q", "-t", "halt", "-s", str(query_pl)],
                    capture_output=True,
                    text=True,
                    timeout=timeout,
                )
                return r.returncode == 0
            except (subprocess.TimeoutExpired, FileNotFoundError):
                return False

    def check_recurrence(self, seq: str, coeffs: list[int], obs: list[tuple[int, int]]) -> tuple[bool, Optional[str]]:
        """
        Load observations into a temp layer and ask holds_rec(Seq, Coeffs).
        Returns (ok, counterexample_str|None).
        """
        # Always compute a Python counterexample for honesty / fallback
        order = len(coeffs)
        cex = None
        for n, v in obs:
            if n < order:
                continue
            pred = 0
            ok_row = True
            for k, c in enumerate(coeffs, start=1):
                # find obs n-k
                prev = dict(obs)
                if n - k not in prev:
                    ok_row = False
                    break
                pred += c * prev[n - k]
            if not ok_row:
                continue
            if pred != v:
                cex = f"n={n}: pred={pred} != obs={v}"
                break
        py_ok = cex is None and any(n >= order for n, _ in obs)

        if self.backend == "swipl" and swipl_available():
            extra = [f"obs({seq}, {n}, {v})" for n, v in obs]
            cstr = "[" + ",".join(str(c) for c in coeffs) + "]"
            ok = self._swipl_query(f"check_rec({seq}, {cstr})", extra_assert=extra)
            # Prefer SWI verdict; keep Python cex if fail
            if ok and not py_ok:
                # inconsistent — trust Python ground check
                return False, cex or "python/swipl disagreement"
            if not ok:
                return False, cex or "finite fail (SWI)"
            return True, None

        # Horn fallback: assert obs + check manually via same Python path
        # (Horn KB does not implement =:= forall; we document this and use
        # the ground Python critic as the resolution of the closed query.)
        return py_ok, cex

    def check_bit_fn(self, kind: str, examples: list[tuple[list[int], int]]) -> tuple[bool, Optional[str]]:
        cex = None
        for bits, out in examples:
            pred = _eval_bits_py(kind, bits)
            if pred != out:
                cex = f"bits={bits}: pred={pred} != {out}"
                break
        py_ok = cex is None and len(examples) > 0

        if self.backend == "swipl" and swipl_available():
            extra = [
                f"holds_bit({i}, [{','.join(map(str, bits))}], {out})"
                for i, (bits, out) in enumerate(examples)
            ]
            # kind may be parity / and_all / xor2 / const(0)
            ok = self._swipl_query(f"check_bit_fn({kind})", extra_assert=extra)
            if not ok:
                return False, cex or "finite fail (SWI)"
            return True, None
        return py_ok, cex

    def check_period(self, seq: str, m: int, period: int, obs: list[tuple[int, int]]) -> tuple[bool, Optional[str]]:
        cex = None
        for n, v in obs:
            if n < period:
                continue
            prev = dict(obs)
            if n - period not in prev:
                continue
            if (v % m) != (prev[n - period] % m):
                cex = f"n={n} mod {m}: {v % m} != {prev[n - period] % m}"
                break
        py_ok = cex is None and period > 0 and any(n >= period for n, _ in obs)

        if self.backend == "swipl" and swipl_available():
            extra = [f"obs({seq}, {n}, {v})" for n, v in obs]
            ok = self._swipl_query(f"check_period({seq}, {m}, {period})", extra_assert=extra)
            if not ok:
                return False, cex or "finite fail (SWI)"
            return True, None
        return py_ok, cex

    def transfer_priors(self, target: str) -> list[tuple[list[int], str]]:
        """Return [(coeffs, source_seq), ...] from archived rec/companion."""
        recs = self.archive.list_recs()
        companions = {
            "fib": {"lucas", "pell"},
            "lucas": {"fib", "pell"},
            "pell": {"fib", "lucas"},
        }
        out = []
        for src, coeffs in recs:
            if src == target:
                continue
            if target in companions.get(src, set()):
                out.append((coeffs, src))
        # Also ask SWI if available (sanity)
        if self.backend == "swipl" and swipl_available():
            # optional; Python parse of theory is source of truth for transfer list
            pass
        return out


def _eval_bits_py(kind: str, bits: list[int]) -> int:
    if kind == "parity":
        r = 0
        for b in bits:
            r ^= b
        return r
    if kind == "and_all":
        return 0 if 0 in bits else 1
    if kind == "xor2":
        if len(bits) >= 2:
            return bits[0] ^ bits[1]
        if len(bits) == 1:
            return bits[0]
        return 0
    if kind.startswith("const(") and kind.endswith(")"):
        return int(kind[6:-1])
    return -1
