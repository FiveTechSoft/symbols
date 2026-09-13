"""
Python kernel: UCB scheduler over (world, family), persistence, tick loop.

Archive + critic are Prolog-shaped (see motor/prolog/). This module does NOT
implement resolution — it schedules which conjecture family to try next.
"""

from __future__ import annotations

import json
import math
import time
from dataclasses import asdict
from pathlib import Path
from typing import Any, Optional

from .prolog.bridge import PrologArchive, PrologCritic, swipl_available
from .worlds import build_worlds
from .worlds.base import FamilySpec, VerifiedFact

MOTOR_DIR = Path(__file__).resolve().parent
RUNS_DIR = MOTOR_DIR / "runs"
UCB_C = 1.4


def _resolve_archive_dir(archive_dir: Path | str | None) -> Path:
    """Resolve archive directory. Default: motor/archive. Relative paths from project root."""
    if archive_dir is None:
        return MOTOR_DIR / "archive"
    p = Path(archive_dir)
    if p.is_absolute():
        p.mkdir(parents=True, exist_ok=True)
        return p
    # Prefer project root (parent of motor/)
    root = MOTOR_DIR.parent
    cand = (root / p).resolve()
    cand.mkdir(parents=True, exist_ok=True)
    return cand


def arm_key(world: str, family: str) -> str:
    return f"{world}::{family}"


class MotorKernel:
    def __init__(self, reset: bool = False, archive_dir: Path | str | None = None) -> None:
        RUNS_DIR.mkdir(parents=True, exist_ok=True)
        self.archive_dir = _resolve_archive_dir(archive_dir)
        theory = self.archive_dir / "theory.pl"
        meta = self.archive_dir / "meta.json"
        if reset:
            if theory.exists():
                theory.unlink()
            if meta.exists():
                meta.unlink()
            skin = self.archive_dir / "skin.json"
            if skin.exists():
                skin.unlink()

        self.archive = PrologArchive(theory_path=theory, meta_path=meta)
        self.critic = PrologCritic(self.archive)
        self.worlds = build_worlds()
        # Share hypothesis language skin across sequence/geometry worlds
        seq = self.worlds.get("sequences")
        geo = self.worlds.get("geometry")
        if seq is not None and geo is not None and hasattr(seq, "language"):
            geo.language = seq.language
        for w in self.worlds.values():
            if hasattr(w, "bind"):
                w.bind(self.archive, self.critic)

        # Restore or init arms from world family catalogs
        self.arms: dict[str, FamilySpec] = {}
        for wname, world in self.worlds.items():
            for fid, fam in world.families().items():
                key = arm_key(wname, fid)
                saved = self.archive.meta.get("arms", {}).get(key)
                if saved:
                    self.arms[key] = FamilySpec.from_dict(saved)
                else:
                    self.arms[key] = fam

        self.total_steps = int(self.archive.meta.get("total_steps", 0))
        self.step_log: list[dict] = list(self.archive.meta.get("step_log", []))
        self.curves: list[dict] = list(self.archive.meta.get("curves", []))
        # Confirmed facts index (Python mirror of verified/1 for quick checks)
        self.prefer_arms: list[str] = []
        self.confirmed: dict[str, VerifiedFact] = {}
        for name in self.archive.verified_names():
            self.confirmed[name] = VerifiedFact(
                name=name,
                family="?",
                world="?",
                formula=name,
                true=True,
                support="restored",
                counterexample=None,
                relation_type="restored",
            )

    def _persist_arms(self) -> None:
        self.archive.meta["arms"] = {k: v.to_dict() for k, v in self.arms.items()}
        self.archive.meta["total_steps"] = self.total_steps
        self.archive.meta["step_log"] = self.step_log[-500:]
        self.archive.meta["curves"] = self.curves
        self.archive.save()

    def ucb_pick(self) -> tuple[str, str, str]:
        """Pick (world, family, why) among unlocked ∧ ¬saturated arms."""
        pool = [a for a in self.arms.values() if a.unlocked and not a.saturated]
        if not pool:
            # try unlock across worlds
            unlocked_any = False
            for wname, world in self.worlds.items():
                # gather this world's families from arms
                wf = {fid: self.arms[arm_key(wname, fid)] for fid in world.families()}
                nxt = world.unlock_next(wf)
                if nxt:
                    self.arms[arm_key(wname, nxt)].unlocked = True
                    unlocked_any = True
            pool = [a for a in self.arms.values() if a.unlocked and not a.saturated]
            if not pool:
                # Eternal curiosity: saturated ≠ dead. Re-open productive arms
                # (novelty stays > 0 in science skins; molt will spawn next).
                for a in self.arms.values():
                    if a.unlocked and not a.dead_end and a.saturated:
                        a.saturated = False
                pool = [a for a in self.arms.values() if a.unlocked and not a.saturated]
            if not pool:
                pool = [a for a in self.arms.values() if a.unlocked]
        if not pool:
            raise RuntimeError("no arms available")

        T = max(1, self.total_steps)
        cands = []
        for fam in pool:
            # find world for why string
            wname = next(k.split("::")[0] for k, v in self.arms.items() if v is fam)
            if fam.n_visits == 0:
                ucb = float("inf")
                why_term = "never-tried → UCB=+∞"
            else:
                bonus = UCB_C * math.sqrt(math.log(T + 1) / fam.n_visits)
                ucb = fam.avg_reward + bonus
                why_term = f"avg={fam.avg_reward:.3f}+{UCB_C}*sqrt(ln({T}+1)/{fam.n_visits})={bonus:.3f}→{ucb:.3f}"
            # Gap-driven bias from BookBrain exam (wrong UNKNOWNs → prefer arm).
            key = arm_key(wname, fam.id)
            if key in getattr(self, "prefer_arms", []):
                ucb = (ucb if ucb != float("inf") else 100.0) + 50.0
                why_term = why_term + f" +gap_bias({key})"
            cands.append((ucb, fam.n_visits, fam.unlock_order, wname, fam.id, why_term))

        def sk(t):
            ucb, nvis, uord, wn, fid, _ = t
            is_inf = 1 if ucb == float("inf") else 0
            finite = 0.0 if ucb == float("inf") else ucb
            return (-is_inf, -finite, nvis, uord, wn, fid)

        cands.sort(key=sk)
        best = cands[0]
        why = (
            f"UCB1 over {len(pool)} arms (world,family); picked {best[3]}::{best[4]} because {best[5]}. "
            f"Rule: argmax[avg + {UCB_C}*sqrt(ln(T+1)/n)]; never-tried=+∞; saturated excluded."
        )
        return best[3], best[4], why

    def _score(self, facts: list[VerifiedFact], fam: FamilySpec, outcomes: list[str]) -> tuple[float, dict]:
        new_true = sum(1 for o in outcomes if o == "new_true")
        new_reject = sum(1 for o in outcomes if o == "new_reject")
        dead_ok = 0
        for f, o in zip(facts, outcomes):
            if fam.dead_end or f.name.startswith("NEG_"):
                if not f.true and o in ("new_reject", "duplicate_reject"):
                    dead_ok += 1
        new_types = len({f.relation_type for f, o in zip(facts, outcomes) if o == "new_true"})
        transfer_hits = sum(1 for f, o in zip(facts, outcomes) if o == "new_true" and f.from_transfer)
        novelty = 1.0 * new_true + 0.5 * new_types + 0.25 * dead_ok + 0.75 * transfer_hits
        cost = 0.05 * len(facts) + 0.02 * fam.param
        score = novelty - cost
        return score, {
            "new_true": new_true,
            "new_reject": new_reject,
            "new_types": new_types,
            "transfer_hits": transfer_hits,
            "dead_ok": dead_ok,
            "cost": round(cost, 4),
            "score": round(score, 4),
        }

    def _ingest(self, fact: VerifiedFact) -> str:
        key = fact.name
        if fact.true:
            if key in self.confirmed:
                return "duplicate_true"
            self.confirmed[key] = fact
            self.archive.assert_verified(fact.world, fact.family, fact.name, fact.formula)
            return "new_true"
        else:
            # reject
            already = any(
                ln.startswith("rejected(") and fact.name in ln for ln in self.archive._lines
            )
            if already:
                return "duplicate_reject"
            why = fact.counterexample or "finite fail"
            self.archive.assert_rejected(fact.name, why)
            return "new_reject"

    def measure_curve_point(self) -> dict:
        from .metrics import compute_metrics

        return compute_metrics(self)

    def tick_once(self) -> dict:
        world_name, fam_id, why = self.ucb_pick()
        fam = self.arms[arm_key(world_name, fam_id)]
        world = self.worlds[world_name]

        conjectures = world.hypothesize(fam, self.confirmed, self.total_steps)
        facts: list[VerifiedFact] = []
        outcomes: list[str] = []
        for conj in conjectures:
            vf = world.verify(conj)
            vf.step = self.total_steps
            outcome = self._ingest(vf)
            facts.append(vf)
            outcomes.append(outcome)

        score, detail = self._score(facts, fam, outcomes)
        fam.n_visits += 1
        fam.total_reward += score
        growth = world.expand_family(fam, detail["new_true"])

        # If all this world's unlocked families saturated → unlock next
        wf = {
            fid: self.arms[arm_key(world_name, fid)]
            for fid in world.families()
        }
        active = [f for f in wf.values() if f.unlocked and not f.saturated]
        unlock_msg = None
        if not active:
            unlock_msg = world.unlock_next(wf)
            if unlock_msg:
                self.arms[arm_key(world_name, unlock_msg)].unlocked = True

        self.total_steps += 1
        entry = {
            "tick": self.total_steps,
            "world": world_name,
            "family": fam_id,
            "why": why,
            "n_conjectures": len(conjectures),
            "outcomes": outcomes,
            "score_detail": detail,
            "growth": growth,
            "unlock": unlock_msg,
            "accepted": [f.name for f, o in zip(facts, outcomes) if o == "new_true"],
            "rejected": [f.name for f, o in zip(facts, outcomes) if o == "new_reject"],
            "transfer": [f.name for f in facts if f.from_transfer and f.true],
        }
        self.step_log.append(entry)
        curve = self.measure_curve_point()
        curve["tick"] = self.total_steps
        self.curves.append(curve)
        self._persist_arms()
        return entry

    def tick(self, steps: int = 40, prefer_arms: list[str] | None = None) -> dict:
        """Run steps. prefer_arms: ['world::family', ...] from exam gaps (no invent)."""
        self.prefer_arms = list(prefer_arms or [])
        t0 = time.time()
        entries = []
        for _ in range(steps):
            entries.append(self.tick_once())
        # baselines once per run batch
        from .metrics import run_baselines, transfer_eval

        baselines = run_baselines(self)
        transfer = transfer_eval(self)
        self.archive.meta["baselines"] = baselines
        self.archive.meta["transfer_eval"] = transfer
        self._persist_arms()

        latest = {
            "ticks_this_run": steps,
            "total_steps": self.total_steps,
            "backend": self.archive.backend,
            "swipl": swipl_available(),
            "n_verified_facts": self.archive.count_verified(),
            "n_rejected": self.archive.count_rejected(),
            "n_lemmas": self.archive.count_lemmas(),
            "n_distinct_types": len(self.archive.distinct_types_approx()),
            "distinct_types": sorted(self.archive.distinct_types_approx()),
            "curves": self.curves,
            "baselines": baselines,
            "transfer_eval": transfer,
            "step_log_tail": entries,
            "elapsed_s": round(time.time() - t0, 3),
            "theory_path": str(self.archive.theory_path),
            "meta_path": str(self.archive.meta_path),
        }
        # flatten last curve metrics to top-level for convenience
        if self.curves:
            for k in (
                "n_verified_facts",
                "n_distinct_types",
                "transfer_accuracy",
                "lemma_reuse_rate",
                "reject_rate",
            ):
                if k in self.curves[-1]:
                    latest[k] = self.curves[-1][k]

        out = RUNS_DIR / "latest.json"
        out.write_text(json.dumps(latest, indent=2), encoding="utf-8")
        return latest

    def status(self) -> dict:
        return {
            "total_steps": self.total_steps,
            "backend": self.archive.backend,
            "swipl": swipl_available(),
            "n_verified_facts": self.archive.count_verified(),
            "n_rejected": self.archive.count_rejected(),
            "n_lemmas": self.archive.count_lemmas(),
            "n_recs": self.archive.list_recs(),
            "n_distinct_types": len(self.archive.distinct_types_approx()),
            "arms": {
                k: {
                    "visits": v.n_visits,
                    "avg_reward": round(v.avg_reward, 4),
                    "unlocked": v.unlocked,
                    "saturated": v.saturated,
                    "param": v.param,
                    "dead_end": v.dead_end,
                }
                for k, v in self.arms.items()
            },
            "theory_preview": "\n".join(self.archive._lines[:40])
            + ("\n..." if len(self.archive._lines) > 40 else ""),
            "theory_path": str(self.archive.theory_path),
            "latest_run": str(RUNS_DIR / "latest.json"),
        }
