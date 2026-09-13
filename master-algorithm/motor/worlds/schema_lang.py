"""
Generative hypothesis language for the sublime motor.

Hypotheses are proposed by OPERATORS (schema classes) whose parameters are
searched — not retrieved from a frozen menu of named theorems (Cassini,
Varignon, …). The language can MUTATE / MOLT: raise order, unlock a new
schema class, widen coeff ranges, add novelty pressure.

Skin = persistable JSON describing which schema classes are live and their
parameters. Critic still gates every fact; this module never writes theory.
"""

from __future__ import annotations

import copy
import json
from dataclasses import asdict, dataclass, field
from itertools import product
from pathlib import Path
from typing import Any, Optional


# Seed schema classes (operators). Names intentionally differ from the old
# hardcoded families() catalog ids where the leap is generative.
SEED_SCHEMAS: dict[str, dict[str, Any]] = {
    "linrec_scan": {
        "description": "Scan small-int linear recurrence coeffs; grow order",
        "order": 1,
        "order_max": 4,
        "coeff_lo": -3,
        "coeff_hi": 3,
        "unlocked": True,
        "origin": "seed",
    },
    "transfer_horn": {
        "description": "Reuse archived rec/2 across companions",
        "unlocked": True,
        "origin": "seed",
    },
    "ratio_scan": {
        "description": "Consecutive-ratio limit hypotheses vs algebraic constants",
        "unlocked": True,
        "origin": "seed",
    },
    "dead_prime": {
        "description": "DEAD END: terms always prime",
        "unlocked": True,
        "dead_end": True,
        "origin": "seed",
    },
    "modperiod_schema": {
        "description": "Pisano-like period schema: search modulus m, discover p",
        "m_max": 4,
        "m_cap": 12,
        "unlocked": False,
        "origin": "seed_locked",
    },
    "bilinear_schema": {
        "description": "Bilinear identity schema: search offset r, sign pattern",
        "r_max": 1,
        "r_cap": 4,
        "unlocked": False,
        "origin": "seed_locked",
    },
    "geo_invent": {
        "description": "Mutate constructions; archive new lemma schemas",
        "mut_depth": 1,
        "mut_cap": 4,
        "unlocked": True,
        "origin": "seed",
    },
}


@dataclass
class SchemaClass:
    id: str
    description: str
    unlocked: bool = True
    dead_end: bool = False
    origin: str = "seed"
    # operator params (interpreted per schema)
    order: int = 1
    order_max: int = 4
    coeff_lo: int = -3
    coeff_hi: int = 3
    m_max: int = 4
    m_cap: int = 12
    r_max: int = 1
    r_cap: int = 4
    mut_depth: int = 1
    mut_cap: int = 4
    # bandit state
    n_visits: int = 0
    total_reward: float = 0.0
    saturated: bool = False
    ticks_no_new_type: int = 0
    novelty_bonus: float = 0.0  # raised on molt to escape [1,1] exploitation

    @property
    def avg_reward(self) -> float:
        if self.n_visits == 0:
            return 0.0
        return self.total_reward / self.n_visits

    def to_dict(self) -> dict:
        return asdict(self)

    @classmethod
    def from_dict(cls, d: dict) -> "SchemaClass":
        keys = cls.__dataclass_fields__
        return cls(**{k: v for k, v in d.items() if k in keys})


@dataclass
class HypothesisLanguage:
    """Growing set of schema classes = the motor's 'skin'."""

    schemas: dict[str, SchemaClass] = field(default_factory=dict)
    generation: int = 0
    molt_history: list[dict] = field(default_factory=list)
    seen_coeff_fingerprints: list[str] = field(default_factory=list)

    @classmethod
    def seed(cls) -> "HypothesisLanguage":
        schemas = {}
        for sid, spec in SEED_SCHEMAS.items():
            schemas[sid] = SchemaClass(id=sid, **spec)
        return cls(schemas=schemas, generation=0)

    def n_unlocked(self) -> int:
        return sum(1 for s in self.schemas.values() if s.unlocked)

    def n_schema_classes(self) -> int:
        return len(self.schemas)

    def unlocked_ids(self) -> list[str]:
        return sorted(s.id for s in self.schemas.values() if s.unlocked)

    def snapshot(self) -> dict:
        return {
            "generation": self.generation,
            "n_schema_classes": self.n_schema_classes(),
            "n_unlocked": self.n_unlocked(),
            "unlocked": self.unlocked_ids(),
            "schemas": {k: v.to_dict() for k, v in self.schemas.items()},
            "seen_coeff_fingerprints": list(self.seen_coeff_fingerprints),
            "molt_history": list(self.molt_history),
        }

    def clone(self) -> "HypothesisLanguage":
        return HypothesisLanguage(
            schemas={k: SchemaClass.from_dict(v.to_dict()) for k, v in self.schemas.items()},
            generation=self.generation,
            molt_history=copy.deepcopy(self.molt_history),
            seen_coeff_fingerprints=list(self.seen_coeff_fingerprints),
        )

    def save_skin(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(self.snapshot(), indent=2), encoding="utf-8")

    @classmethod
    def load_skin(cls, path: Path) -> Optional["HypothesisLanguage"]:
        if not path.exists():
            return None
        data = json.loads(path.read_text(encoding="utf-8"))
        schemas = {k: SchemaClass.from_dict(v) for k, v in data.get("schemas", {}).items()}
        return cls(
            schemas=schemas,
            generation=int(data.get("generation", 0)),
            molt_history=list(data.get("molt_history", [])),
            seen_coeff_fingerprints=list(data.get("seen_coeff_fingerprints", [])),
        )

    def note_coeffs(self, coeffs: list[int]) -> None:
        fp = ",".join(map(str, coeffs))
        if fp not in self.seen_coeff_fingerprints:
            self.seen_coeff_fingerprints.append(fp)

    def preferred_novel_coeffs(self) -> bool:
        """True if novelty bonus says prefer non-[1,1] fits when available."""
        return any(s.novelty_bonus > 0 for s in self.schemas.values() if s.unlocked)

    # --- mutation / molt operators ---

    def mutate(self, reason: str) -> dict:
        """
        Apply one language mutation. Returns a description dict.
        Does NOT write theory facts — only changes the hypothesis skin.
        """
        before = self.unlocked_ids()
        actions: list[str] = []

        # 1) Raise linrec order if at plateau
        lr = self.schemas.get("linrec_scan")
        if lr and lr.unlocked and lr.order < lr.order_max:
            old = lr.order
            lr.order = min(lr.order_max, lr.order + 1)
            lr.saturated = False
            lr.ticks_no_new_type = 0
            actions.append(f"raise_linrec_order {old}→{lr.order}")

        # 2) Widen coeff range slightly
        if lr and lr.unlocked and abs(lr.coeff_lo) < 5:
            lr.coeff_lo -= 1
            lr.coeff_hi += 1
            lr.saturated = False
            actions.append(f"widen_coeff_range [{lr.coeff_lo},{lr.coeff_hi}]")

        # 3) Unlock locked schema by evidence/pressure
        locked = [s for s in self.schemas.values() if not s.unlocked]
        if locked:
            # Prefer generative schemas over dead ends
            locked.sort(key=lambda s: (1 if s.dead_end else 0, s.id))
            nxt = locked[0]
            nxt.unlocked = True
            nxt.saturated = False
            nxt.origin = f"mutation:unlocked_gen{self.generation + 1}"
            actions.append(f"unlock_schema {nxt.id}")

        # 4) Grow modperiod / bilinear params if already unlocked
        mp = self.schemas.get("modperiod_schema")
        if mp and mp.unlocked and mp.m_max < mp.m_cap:
            old = mp.m_max
            mp.m_max = min(mp.m_cap, mp.m_max + 2)
            mp.saturated = False
            actions.append(f"raise_m_max {old}→{mp.m_max}")

        bi = self.schemas.get("bilinear_schema")
        if bi and bi.unlocked and bi.r_max < bi.r_cap:
            old = bi.r_max
            bi.r_max = min(bi.r_cap, bi.r_max + 1)
            bi.saturated = False
            actions.append(f"raise_r_max {old}→{bi.r_max}")

        # 5) Grow geo invent depth
        gi = self.schemas.get("geo_invent")
        if gi and gi.unlocked and gi.mut_depth < gi.mut_cap:
            old = gi.mut_depth
            gi.mut_depth = min(gi.mut_cap, gi.mut_depth + 1)
            gi.saturated = False
            actions.append(f"raise_mut_depth {old}→{gi.mut_depth}")

        # 6) Novelty bonus so search doesn't only exploit [1,1]
        structural = [a for a in actions if not a.startswith("novelty")]
        if lr:
            lr.novelty_bonus = min(2.0, lr.novelty_bonus + 0.5)
            actions.append(f"novelty_bonus→{lr.novelty_bonus}")

        # 7) If no structural growth left, spawn a fresh schema class (operator variant)
        if not structural:
            # Prefer bilinear/modperiod depth variants, else linrec mutant
            bi = self.schemas.get("bilinear_schema")
            mp = self.schemas.get("modperiod_schema")
            if bi and bi.unlocked:
                new_id = f"bilinear_schema_r{bi.r_max}_g{self.generation + 1}"
                if new_id not in self.schemas:
                    self.schemas[new_id] = SchemaClass(
                        id=new_id,
                        description=f"Mutant bilinear schema r_max={bi.r_max}",
                        unlocked=True,
                        origin=f"mutation:spawn_gen{self.generation + 1}",
                        r_max=bi.r_max,
                        r_cap=bi.r_cap,
                    )
                    actions.append(f"spawn_schema {new_id}")
            elif mp and mp.unlocked:
                new_id = f"modperiod_schema_m{mp.m_max}_g{self.generation + 1}"
                if new_id not in self.schemas:
                    self.schemas[new_id] = SchemaClass(
                        id=new_id,
                        description=f"Mutant modperiod m_max={mp.m_max}",
                        unlocked=True,
                        origin=f"mutation:spawn_gen{self.generation + 1}",
                        m_max=mp.m_max,
                        m_cap=mp.m_cap,
                    )
                    actions.append(f"spawn_schema {new_id}")
            elif lr:
                new_id = f"linrec_scan_o{lr.order}_v{self.generation + 1}"
                if new_id not in self.schemas:
                    self.schemas[new_id] = SchemaClass(
                        id=new_id,
                        description=f"Mutant linrec scan at order≥{lr.order}",
                        unlocked=True,
                        origin=f"mutation:spawn_gen{self.generation + 1}",
                        order=lr.order,
                        order_max=lr.order_max,
                        coeff_lo=lr.coeff_lo,
                        coeff_hi=lr.coeff_hi,
                        novelty_bonus=1.0,
                    )
                    actions.append(f"spawn_schema {new_id}")

        self.generation += 1
        # Unsaturate productive arms so UCB revisits after molt
        for s in self.schemas.values():
            if s.unlocked and not s.dead_end:
                s.saturated = False

        info = {
            "generation": self.generation,
            "reason": reason,
            "actions": actions,
            "unlocked_before": before,
            "unlocked_after": self.unlocked_ids(),
            "n_schema_classes": self.n_schema_classes(),
        }
        self.molt_history.append(info)
        return info


# ---------------------------------------------------------------------------
# Generators: operators → candidate payloads (not named theorems)
# ---------------------------------------------------------------------------

def generate_linrec_candidates(
    vals: list[int],
    schema: SchemaClass,
    seq: str,
    prefer_novel: bool = False,
    seen: Optional[list[str]] = None,
) -> list[dict]:
    """Scan coeff space for orders 1..schema.order; return payload dicts."""
    out = []
    seen = seen or []
    coeff_range = range(schema.coeff_lo, schema.coeff_hi + 1)
    for order in range(1, schema.order + 1):
        fits = _scan_recurrence(vals, order, coeff_range)
        if not fits:
            out.append(
                {
                    "kind": "rec",
                    "seq": seq,
                    "coeffs": None,
                    "order": order,
                    "schema": schema.id,
                    "formula": f"no small-int rec order {order} on {seq} in [{schema.coeff_lo},{schema.coeff_hi}]",
                    "name": f"rec_{seq}_o{order}_none_{schema.id}",
                    "relation_type": f"linrec:{schema.id}",
                }
            )
            continue
        # Sort: if novelty preferred, deprioritize fingerprints already seen / [1,1]
        def rank(item):
            l1, coeffs = item
            fp = ",".join(map(str, coeffs))
            novel_pen = 0
            if prefer_novel:
                if fp in seen:
                    novel_pen += 10
                if coeffs == [1, 1] or coeffs == [1, 1][:order]:
                    novel_pen += 5
            return (novel_pen, l1)

        fits_sorted = sorted(fits, key=rank)
        # Emit best + optionally a novel alternative
        chosen = [fits_sorted[0]]
        if prefer_novel and len(fits_sorted) > 1:
            for alt in fits_sorted[1:]:
                fp = ",".join(map(str, alt[1]))
                if fp not in seen and alt[1] != fits_sorted[0][1]:
                    chosen.append(alt)
                    break
        for _, coeffs in chosen[:2]:
            formula = f"{seq}(n) = " + " + ".join(
                f"({c})*{seq}(n-{i+1})" for i, c in enumerate(coeffs)
            )
            out.append(
                {
                    "kind": "rec",
                    "seq": seq,
                    "coeffs": coeffs,
                    "order": order,
                    "schema": schema.id,
                    "formula": formula,
                    "name": f"rec_{seq}_o{order}_{'_'.join(map(str, coeffs))}",
                    "relation_type": f"linrec:{schema.id}",
                }
            )
    return out


def _scan_recurrence(
    vals: list[int], order: int, coeff_range
) -> list[tuple[int, list[int]]]:
    N = len(vals) - 1
    if N < order + 2:
        return []
    fits = []
    for coeffs in product(coeff_range, repeat=order):
        if all(c == 0 for c in coeffs):
            continue
        ok = True
        for n in range(order, N + 1):
            pred = sum(coeffs[i] * vals[n - 1 - i] for i in range(order))
            if pred != vals[n]:
                ok = False
                break
        if ok:
            fits.append((sum(abs(c) for c in coeffs), list(coeffs)))
    return fits


def generate_modperiod_candidates(
    vals: list[int],
    schema: SchemaClass,
    seq: str,
    N: int,
    pisano_fn,
) -> list[dict]:
    out = []
    for m in range(2, schema.m_max + 1):
        period = pisano_fn(vals, m, N)
        out.append(
            {
                "kind": "period",
                "seq": seq,
                "m": m,
                "period": period,
                "schema": schema.id,
                "formula": f"π_{seq}({m})={period}" if period else f"π_{seq}({m})=unknown",
                "name": f"period_{seq}_m{m}",
                "relation_type": f"modperiod:{schema.id}",
            }
        )
    return out


def generate_bilinear_candidates(
    vals: list[int],
    schema: SchemaClass,
    seq: str,
    N: int,
) -> list[dict]:
    """
    Bilinear SCHEMA: search parameter r for
      vals[n]^2 - vals[n+r]*vals[n-r]  ?=?  (-1)^{n-r} * vals[r]^2
    and the Cassini-shaped offset form
      vals[n+1]*vals[n-1] - vals[n]^2  ?=?  (-1)^n
    Also emit a deliberately wrong schema (r with bogus RHS) for critic reject.
    """
    out = []
    # Offset form (Cassini-shaped) — parameters fixed by schema algebra, not a canned name
    out.append(
        {
            "kind": "bilinear",
            "seq": seq,
            "form": "offset_pm1",
            "r": 1,
            "schema": schema.id,
            "formula": f"{seq}(n+1){seq}(n-1)-{seq}(n)^2 = (-1)^n",
            "name": f"bilin_{seq}_offset_pm1",
            "relation_type": f"bilinear:{schema.id}",
        }
    )
    for r in range(1, schema.r_max + 1):
        out.append(
            {
                "kind": "bilinear",
                "seq": seq,
                "form": "catalan_like",
                "r": r,
                "schema": schema.id,
                "formula": (
                    f"{seq}(n)^2 - {seq}(n+{r}){seq}(n-{r}) = (-1)^(n-{r}) {seq}({r})^2"
                ),
                "name": f"bilin_{seq}_r{r}",
                "relation_type": f"bilinear:{schema.id}",
            }
        )
    # Dead-end variant of the schema (wrong RHS constant) — must be rejectable
    out.append(
        {
            "kind": "bilinear",
            "seq": seq,
            "form": "bogus_const",
            "r": 1,
            "schema": schema.id,
            "formula": f"{seq}(n+1){seq}(n-1)-{seq}(n)^2 = 2  (bogus)",
            "name": f"bilin_{seq}_bogus_const2",
            "relation_type": f"bilinear_neg:{schema.id}",
        }
    )
    return out


def verify_bilinear(vals: list[int], payload: dict, N: int) -> tuple[bool, str, Optional[str]]:
    form = payload.get("form")
    r = int(payload.get("r", 1))
    if form == "offset_pm1":
        fails = []
        ok_n = 0
        for n in range(1, N):
            if n + 1 > N:
                break
            lhs = vals[n + 1] * vals[n - 1] - vals[n] ** 2
            rhs = (-1) ** n
            if lhs == rhs:
                ok_n += 1
            else:
                fails.append(n)
                break
        true = len(fails) == 0 and ok_n > 0
        return (
            true,
            f"{ok_n} checks offset_pm1" if true else "finite fail",
            None if true else f"fail n={fails[0]} lhs≠(-1)^n",
        )
    if form == "catalan_like":
        if N < 2 * r + 2:
            return False, "prefix too short", "insufficient N"
        fails = []
        ok_n = 0
        for n in range(r, N - r + 1):
            lhs = vals[n] ** 2 - vals[n + r] * vals[n - r]
            rhs = ((-1) ** (n - r)) * (vals[r] ** 2)
            if lhs == rhs:
                ok_n += 1
            else:
                fails.append(n)
                break
        true = len(fails) == 0 and ok_n > 0
        return (
            true,
            f"{ok_n} checks r={r}" if true else "finite fail",
            None if true else f"fail n={fails[0]}",
        )
    if form == "bogus_const":
        fails = []
        for n in range(1, min(N, 8)):
            if n + 1 > N:
                break
            lhs = vals[n + 1] * vals[n - 1] - vals[n] ** 2
            if lhs != 2:
                fails.append(n)
                break
        # Expect rejection
        true = len(fails) == 0
        return (
            true,
            "unexpected hold" if true else "finite fail (expected)",
            None if true else f"n={fails[0]}: lhs={vals[fails[0]+1]*vals[fails[0]-1]-vals[fails[0]]**2}≠2",
        )
    return False, "unknown bilinear form", "unknown"
