#!/usr/bin/env python3
"""
Science worlds runner — scratch archive only (mouth archive untouched).

Standing loop (philosophy — there is no done):
  tick → compare/extrapolate → if plateau then molt/spawn → forever

A finite --max-molts is only a single-run cap, not the end of curiosity.
UCB never disables all arms; saturated families molt instead of dying;
novelty_bonus stays > 0.

  cd /workspace/master-algorithm
  python -m motor.run_sciences --ticks 8 --max-molts 4
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

MOTOR_DIR = Path(__file__).resolve().parent
SCRATCH = MOTOR_DIR / "archive-sciences"
RUNS = MOTOR_DIR / "runs"
SEED_SCIENCE = None  # filled lazily


STEM_WORLDS = (
    "symmetry", "chance", "info",
    "astro", "algebra", "calculus", "nets",
    "electro", "physics", "chem",
)


def _prefer_science_arms(kernel) -> list[str]:
    keys = []
    for k, fam in kernel.arms.items():
        w = k.split("::")[0]
        if w in STEM_WORLDS or w == "logic":
            keys.append(k)
        if "transfer" in k or "compare" in k or "extrapolate" in k:
            if k not in keys:
                keys.append(k)
    return keys


def _science_worlds(kernel):
    return {n: kernel.worlds[n] for n in STEM_WORLDS if n in kernel.worlds}


def _molt_sciences(kernel, reason: str) -> list[dict]:
    """Molt each science skin: spawn non-seed classes, reopen arms, novelty floor."""
    from motor.kernel import arm_key

    logs = []
    for name, world in _science_worlds(kernel).items():
        lang = getattr(world, "language", None)
        if lang is None:
            continue
        info = lang.molt(reason)
        world.persist_skin()
        if hasattr(world, "sync_arms"):
            world.sync_arms(kernel.arms)
        # Also register any brand-new family ids into kernel.arms
        for fid, fam in world.families().items():
            key = arm_key(name, fid)
            if key not in kernel.arms:
                kernel.arms[key] = fam
            else:
                kernel.arms[key].unlocked = fam.unlocked
                kernel.arms[key].saturated = False  # reopen — no done
                kernel.arms[key].param = fam.param
                kernel.arms[key].param_max = fam.param_max
        # Persist schema/2-style meta (not verified theorems)
        for s in lang.schemas.values():
            flag = "true" if s.unlocked else "false"
            kernel.archive.append_clause(
                f"schema({s.id}, unlocked({flag})).",
                comment=f"science skin {name} gen={lang.generation}",
            )
        logs.append(info)
    kernel._persist_arms()
    return logs


def _molt_sequences_too(kernel, reason: str) -> dict | None:
    """Also grow sequences language so bilinear_*_g* emergence can appear."""
    seq = kernel.worlds.get("sequences")
    if seq is None or not hasattr(seq, "language"):
        return None
    from motor.molt import molt_once

    return molt_once(kernel, reason)


def _metrics(kernel) -> dict:
    from motor.metrics import compute_metrics

    m = compute_metrics(kernel)
    science = {}
    spawned = []
    for name, world in _science_worlds(kernel).items():
        lang = getattr(world, "language", None)
        if not lang:
            continue
        science[name] = {
            "generation": lang.generation,
            "n_schema_classes": lang.n_schema_classes(),
            "unlocked": lang.unlocked_ids(),
            "spawned": lang.spawned_ids(),
        }
        spawned.extend(lang.spawned_ids())
    # sequences spawned
    seq = kernel.worlds.get("sequences")
    seq_spawned = []
    if seq and hasattr(seq, "language"):
        seed_ids = set(getattr(__import__("motor.worlds.schema_lang", fromlist=["SEED_SCHEMAS"]).SEED_SCHEMAS, "keys", lambda: [])())
        try:
            from motor.worlds.schema_lang import SEED_SCHEMAS

            seed_ids = set(SEED_SCHEMAS.keys())
        except Exception:
            seed_ids = set()
        seq_spawned = [sid for sid in seq.language.schemas if sid not in seed_ids]
        science["sequences"] = {
            "generation": seq.language.generation,
            "n_schema_classes": seq.language.n_schema_classes(),
            "spawned": seq_spawned,
        }
        spawned.extend(seq_spawned)

    compare = {}
    for name, world in _science_worlds(kernel).items():
        log = getattr(world, "compare_log", [])
        hits = sum(1 for e in log if e.get("hit"))
        compare[name] = {"attempts": len(log), "hits": hits, "misses": len(log) - hits}

    return {
        **m,
        "science": science,
        "spawned_schemas": sorted(set(spawned)),
        "n_spawned": len(set(spawned)),
        "compare": compare,
        "n_verified": m["n_verified_facts"],
        "n_types": m["n_distinct_types"],
    }


def _explicit_compare_pass(kernel) -> dict:
    """After ticks: force transfer_prior across science worlds; log hits/misses."""
    results = {"attempts": [], "hits": 0, "misses": 0}
    for name, world in list(_science_worlds(kernel).items()) + [
        (n, kernel.worlds[n]) for n in ("logic", "sequences") if n in kernel.worlds
    ]:
        if not hasattr(world, "transfer_prior"):
            continue
        conjs = world.transfer_prior(kernel.confirmed)
        for conj in conjs[:8]:
            vf = world.verify(conj)
            vf.step = kernel.total_steps
            # ingest without counting as a full UCB tick — still critic-gated
            outcome = kernel._ingest(vf)
            hit = bool(vf.true and outcome in ("new_true", "duplicate_true"))
            entry = {
                "world": name,
                "name": vf.name,
                "true": vf.true,
                "outcome": outcome,
                "from_transfer": vf.from_transfer,
                "transfer_source": vf.transfer_source,
                "support": vf.support,
                "counterexample": vf.counterexample,
            }
            results["attempts"].append(entry)
            if vf.true:
                results["hits"] += 1
            else:
                results["misses"] += 1
    kernel._persist_arms()
    return results


def _example_clauses(kernel, n: int = 8) -> list[dict]:
    import re
    lines = kernel.archive._lines
    verified = [ln for ln in lines if ln.startswith("verified(")]
    rejected = [ln for ln in lines if ln.startswith("rejected(")]
    bit_fns = [ln for ln in lines if ln.startswith("bit_fn(")]
    schemas = [ln for ln in lines if ln.startswith("schema(")]

    picks: list[dict] = []

    def add(status, line, reason):
        if len(picks) >= n:
            return
        if any(p["clause"] == line[:220] for p in picks):
            return
        picks.append({"status": status, "clause": line[:220], "reason": reason})

    # 1 emergent verified
    for ln in verified:
        if re.search(r"verified\(fact\(\w+,\s*\w+_n\d+_g\d+", ln):
            add("verified_emergent", ln, "verified under NON-SEED spawned schema; critic accepted")
            break
    # 2 science verified (non-transfer)
    for ln in verified:
        if any(w in ln for w in STEM_WORLDS) and "transfer_" not in ln and "extrap_" not in ln:
            add("verified", ln, "science world; critic numeric/table gate accepted")
        if len([p for p in picks if p["status"] == "verified"]) >= 2:
            break
    # 2 COMPARE/EXTRAPOLATE
    n_te = 0
    for ln in verified:
        if "transfer_" in ln or "extrap_" in ln:
            add("verified", ln, "COMPARE/EXTRAPOLATE form; critic accepted")
            n_te += 1
        if n_te >= 2:
            break
    if bit_fns:
        add("verified_bit_fn", bit_fns[0], "logic bit_fn prior available for cross-world transfer")
    # ≥2 rejected — prefer science dead-ends / broken tables
    prefer = ("NEG_chance", "NEG_info", "NEG_sym", "NEG_astro", "NEG_alg", "NEG_calc", "NEG_nets", "NEG_electro", "NEG_phys", "NEG_chem", "bayes_swap", "z2_claim_mult", "product_mod2", "XOR_linear", "kepler_wrong")
    sci_rej = [ln for ln in rejected if any(s in ln for s in prefer)]
    sci_rej += [ln for ln in rejected if "wait_no_bitfn" in ln and ln not in sci_rej]
    for ln in sci_rej:
        add("rejected", ln, "science dead-end / broken hypothesis; critic finite-fail")
        if len([p for p in picks if p["status"] == "rejected"]) >= 2:
            break
    for ln in rejected:
        if len([p for p in picks if p["status"] == "rejected"]) >= 2:
            break
        add("rejected", ln, "critic rejected")
    for ln in schemas:
        if "_g" in ln and any(p in ln for p in ("sym_", "chance_", "info_", "astro_", "alg_", "calc_", "nets_", "electro_", "phys_", "chem_", "bilinear")):
            add("skin_meta_emergent", ln, "schema/2 for NON-SEED spawned class (not a verified theorem)")
            break
    return picks[:n]


def _beyond_limits_clause(kernel) -> dict | None:
    """Name a verified clause whose family/schema was not in the original seed catalogs."""
    from motor.worlds.science_lang import SCIENCE_SEED
    from motor.worlds.schema_lang import SEED_SCHEMAS

    seed_fams = set(SCIENCE_SEED.keys()) | set(SEED_SCHEMAS.keys())
    # also original logic/geometry family ids
    seed_fams |= {
        "boolean_from_examples", "dead_end_const_true",
        "linear_recurrences", "transfer_recurrence", "ratio_limits",
        "dead_end_always_prime", "modular_periods", "bilinear_schema",
    }
    for ln in kernel.archive._lines:
        if not ln.startswith("verified("):
            continue
        # verified(fact(World, Family, Name, Formula)).
        import re

        m = re.match(r"verified\(fact\((\w+),\s*(\w+),", ln)
        if not m:
            continue
        world, fam = m.group(1), m.group(2)
        if fam not in seed_fams and ("_g" in fam or "_n" in fam):
            return {
                "clause": ln[:240],
                "world": world,
                "family": fam,
                "note": "family/schema id not in original seed catalogs — emerged via molt/spawn",
            }
    # fallback: any verified under a spawned science schema present in skins
    for name, world in _science_worlds(kernel).items():
        lang = getattr(world, "language", None)
        if not lang:
            continue
        for sid in lang.spawned_ids():
            for ln in kernel.archive._lines:
                if ln.startswith("verified(") and sid in ln:
                    return {
                        "clause": ln[:240],
                        "world": name,
                        "family": sid,
                        "note": "verified under spawned non-seed schema",
                    }
    return None


def run(archive_dir=None, ticks_per_batch=8, max_molts=4, reset=True) -> dict:
    from motor.kernel import MotorKernel

    archive_dir = archive_dir or SCRATCH
    t0 = time.time()
    kernel = MotorKernel(reset=reset, archive_dir=archive_dir)

    prefer = _prefer_science_arms(kernel)
    molt_log: list[dict] = []
    batch_log: list[dict] = []
    compare_passes: list[dict] = []

    before = _metrics(kernel)

    # Warmup
    warm = kernel.tick(steps=ticks_per_batch, prefer_arms=prefer)
    batch_log.append({"phase": "warmup", "metrics": _metrics(kernel)})
    cmp0 = _explicit_compare_pass(kernel)
    compare_passes.append({"phase": "post_warmup", **{k: cmp0[k] for k in ("hits", "misses")}})
    # re-prefer after new arms
    prefer = _prefer_science_arms(kernel)

    for i in range(max_molts):
        cur = _metrics(kernel)
        reason = f"plateau/curiosity molt {i+1}/{max_molts} (run cap — philosophy is forever)"
        sci_logs = _molt_sciences(kernel, reason)
        seq_info = None
        # Also molt sequences every other molt so non-seed bilinear_* can appear
        if i % 2 == 0:
            try:
                seq_info = _molt_sequences_too(kernel, reason)
            except Exception as e:
                seq_info = {"error": str(e)}

        prefer = _prefer_science_arms(kernel)
        batch = kernel.tick(steps=ticks_per_batch, prefer_arms=prefer)
        after = _metrics(kernel)
        cmp_i = _explicit_compare_pass(kernel)
        compare_passes.append({"phase": f"post_molt_{i}", "hits": cmp_i["hits"], "misses": cmp_i["misses"]})

        entry = {
            "molt_index": i,
            "reason": reason,
            "science_molts": sci_logs,
            "sequences_molt": seq_info.get("actions") if isinstance(seq_info, dict) else seq_info,
            "metrics_after": after,
            "n_spawned": after.get("n_spawned", 0),
            "spawned": after.get("spawned_schemas", []),
        }
        molt_log.append(entry)
        batch_log.append({"phase": f"post_molt_{i}", "metrics": after})

        # Keep going until run cap — never declare done
        prefer = _prefer_science_arms(kernel)

    final = _metrics(kernel)
    beyond = _beyond_limits_clause(kernel)
    examples = _example_clauses(kernel, 8)

    # Collect compare details from last pass
    last_compare = _explicit_compare_pass(kernel)

    result = {
        "archive_dir": str(kernel.archive_dir),
        "ticks_per_batch": ticks_per_batch,
        "max_molts": max_molts,
        "philosophy": "tick → compare/extrapolate → if plateau then molt/spawn → forever",
        "run_cap_note": (
            "max_molts is a single-run cap only. UCB reopens saturated productive arms; "
            "science skins keep novelty_bonus ≥ floor; molt spawns non-seed classes."
        ),
        "metrics_before": before,
        "metrics_final": final,
        "molt_log": molt_log,
        "batch_log": batch_log,
        "compare_passes": compare_passes,
        "compare_detail_tail": last_compare["attempts"][-12:],
        "compare_totals": {"hits": last_compare["hits"], "misses": last_compare["misses"]},
        "example_clauses": examples,
        "beyond_limits": beyond,
        "n_verified": final.get("n_verified"),
        "n_rejected": final.get("n_rejected"),
        "n_types": final.get("n_types"),
        "n_spawned": final.get("n_spawned"),
        "spawned_schemas": final.get("spawned_schemas"),
        "elapsed_s": round(time.time() - t0, 3),
        "human_seed": (
            "Operator catalogs (SCIENCE_SEED / SEED_SCHEMAS), UCB constant, epsilon, "
            "finite-table generators, and the standing loop itself are human-authored. "
            "Every verified/1 still requires critic acceptance on obs."
        ),
    }

    RUNS.mkdir(parents=True, exist_ok=True)
    (RUNS / "sciences.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    return result


def write_report(result: dict) -> str:
    final = result["metrics_final"]
    lines = []
    lines.append("# 12 — Sciences: transfer of form toward a multiverse")
    lines.append("")
    lines.append("**Scratch archive:** `motor/archive-sciences/` (live mouth archive untouched).")
    lines.append("**Doctrine:** no cosmology essays, no hardcoded “the multiverse is…”.")
    lines.append("A science exists here only as `obs → conjecture → critic → verified/rejected`.")
    lines.append("")
    lines.append("## Standing loop (no done)")
    lines.append("")
    lines.append("```")
    lines.append("tick → compare/extrapolate → if plateau then molt/spawn → forever")
    lines.append("```")
    lines.append("")
    lines.append(
        f"This run used `--max-molts {result['max_molts']}` as a **single-run cap only**, "
        "not the philosophy. UCB never disables all productive arms (saturated → reopen); "
        "science skins keep `novelty_bonus ≥ novelty_floor`; a saturated family **molts/spawns** "
        "instead of dying."
    )
    lines.append("")
    lines.append("## Worlds")
    lines.append("")
    lines.append("| world | role |")
    lines.append("|-------|------|")
    lines.append("| sequences | Fib/Lucas/Pell — form transfer already (rec priors) |")
    lines.append("| logic | bit_fn from examples (parity / and_all / xor2) — priors for COMPARE |")
    lines.append("| geometry | kept; lemma reuse path |")
    lines.append("| symmetry | discrete invariants, Z2/Klein tables, involution |")
    lines.append("| chance | generated Bayes/log-odds/entropy identities (eps critic) |")
    lines.append("| info | MI/independence on tiny joints; transfers bit_fn — does not re-learn XOR/AND |")
    lines.append("")
    lines.append("## Metrics (this run)")
    lines.append("")
    lines.append(f"- n_verified: **{result.get('n_verified')}**")
    lines.append(f"- n_rejected: **{result.get('n_rejected')}**")
    lines.append(f"- n_types: **{result.get('n_types')}**")
    lines.append(f"- n_spawned (non-seed schemas): **{result.get('n_spawned')}** → `{result.get('spawned_schemas')}`")
    lines.append(f"- compare totals (last pass): hits={result.get('compare_totals', {}).get('hits')} "
                 f"misses={result.get('compare_totals', {}).get('misses')}")
    lines.append(f"- elapsed_s: {result.get('elapsed_s')}")
    lines.append("")
    sci = final.get("science") or {}
    if sci:
        lines.append("### Per-world skin")
        lines.append("")
        for w, st in sci.items():
            lines.append(
                f"- **{w}**: gen={st.get('generation')} classes={st.get('n_schema_classes')} "
                f"spawned={st.get('spawned')}"
            )
        lines.append("")
    lines.append("## Beyond limits (emergent schema)")
    lines.append("")
    beyond = result.get("beyond_limits")
    if beyond:
        lines.append(
            f"Verified/associated clause under **non-seed** family `{beyond.get('family')}` "
            f"(world `{beyond.get('world')}`):"
        )
        lines.append("")
        lines.append(f"```")
        lines.append(beyond.get("clause", ""))
        lines.append(f"```")
        lines.append("")
        lines.append(beyond.get("note", ""))
    else:
        lines.append(
            "Honest limit: this run may have spawned schemas without yet parking a "
            "`verified/1` under the new id (UCB still exploring). Spawned ids are listed "
            "above; critic still gates any future facts under them."
        )
    lines.append("")
    lines.append("## 8 example clauses")
    lines.append("")
    for i, ex in enumerate(result.get("example_clauses") or [], 1):
        lines.append(f"{i}. **{ex.get('status')}** — {ex.get('reason')}")
        lines.append(f"   `{ex.get('clause')}`")
    lines.append("")
    lines.append("## COMPARE / EXTRAPOLATE (hits and honest misses)")
    lines.append("")
    for e in (result.get("compare_detail_tail") or [])[:8]:
        mark = "HIT" if e.get("true") else "MISS"
        lines.append(
            f"- {mark} `{e.get('name')}` ← {e.get('transfer_source')} "
            f"({e.get('support') or e.get('counterexample')})"
        )
    lines.append("")
    lines.append("## What is still human seed")
    lines.append("")
    lines.append(result.get("human_seed", ""))
    lines.append("")
    lines.append("## Why this is the path to “multiverse”")
    lines.append("")
    lines.append(
        "Here, understanding a multiverse means **the same form transfers across worlds** "
        "(Fib→Lucas already; now invariant/bit_fn → symmetry, identity → larger chance tables, "
        "bit_fn → MI structure). Mouth correctly stays UNKNOWN on the word “multiverso” until "
        "atoms exist — we do not dump cosmology into `theory.pl`."
    )
    lines.append("")
    lines.append("## Honesty / limits")
    lines.append("")
    lines.append("- Generators, epsilon, and operator seeds are human-authored.")
    lines.append("- Dead-end families must fail (curiosity tax).")
    lines.append("- Finite run cap ≠ finished learning.")
    lines.append("- Zero invented cosmology facts.")
    lines.append("")
    text = "\n".join(lines) + "\n"
    out = ROOT / "12-sciences.md"
    out.write_text(text, encoding="utf-8")
    return text


def main(argv=None) -> int:
    p = argparse.ArgumentParser(description="Science worlds + eternal curiosity loop")
    p.add_argument("--archive", default="motor/archive-sciences")
    p.add_argument("--ticks", type=int, default=8, help="ticks per batch")
    p.add_argument("--max-molts", type=int, default=4, help="single-run cap only")
    p.add_argument("--no-reset", action="store_true")
    args = p.parse_args(argv)

    # Sanity: default live archive still works
    from motor.kernel import MotorKernel

    try:
        k0 = MotorKernel(reset=False)
        _ = k0.status()
    except Exception as e:
        print(f"warning: default kernel probe: {e}", file=sys.stderr)

    result = run(
        archive_dir=args.archive,
        ticks_per_batch=args.ticks,
        max_molts=args.max_molts,
        reset=not args.no_reset,
    )
    report = write_report(result)
    print(
        json.dumps(
            {
                "n_verified": result.get("n_verified"),
                "n_rejected": result.get("n_rejected"),
                "n_types": result.get("n_types"),
                "n_spawned": result.get("n_spawned"),
                "spawned": result.get("spawned_schemas"),
                "beyond_limits_family": (result.get("beyond_limits") or {}).get("family"),
                "compare_hits": result.get("compare_totals", {}).get("hits"),
                "compare_misses": result.get("compare_totals", {}).get("misses"),
                "elapsed_s": result.get("elapsed_s"),
                "wrote": ["motor/runs/sciences.json", "12-sciences.md"],
                "report_chars": len(report),
            },
            indent=2,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
