#!/usr/bin/env python3
"""Run the first Master-Algorithm experimental battery.

Usage (from repo root or this dir):
  python experiments/run_battery.py
"""
from __future__ import annotations

import json
import sys
import time
from pathlib import Path

import numpy as np

# Allow running from repo root or experiments/
HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(HERE))

from tasks import (
    make_symbolist,
    make_connectionist,
    make_evolutionary,
    make_bayesian,
    make_analogizer,
)
from learners import (
    specialized_symbolist,
    specialized_connectionist,
    specialized_evolutionary,
    specialized_bayesian,
    specialized_analogizer,
    UnifierMLP,
)

SEED = 0
RESULTS_PATH = HERE / "results.json"


def _winner(spec_primary, uni_primary, sense="maximize", tol=1e-9):
    if sense == "minimize":
        if abs(spec_primary - uni_primary) <= tol:
            return "tie"
        return "specialized" if spec_primary < uni_primary else "unifier"
    if abs(spec_primary - uni_primary) <= tol:
        return "tie"
    return "specialized" if spec_primary > uni_primary else "unifier"


def run_all():
    np.random.seed(SEED)
    t0 = time.time()
    results = {
        "seed": SEED,
        "unifier": {
            "name": UnifierMLP.name,
            "class": "sklearn MLPClassifier/MLPRegressor (32,16)",
            "rationale": (
                "Connectionist substrate as Domingos-style unification candidate: "
                "force one MLP algorithm class onto all five tribe tasks via "
                "encodings/surrogates/amortization — honest about where it loses."
            ),
        },
        "tasks": {},
    }

    # ---- 1 Symbolist ----
    print("== Symbolist ==")
    Xs_tr, ys_tr, Xs_te, ys_te, s_meta = make_symbolist(seed=SEED)
    spec = specialized_symbolist(Xs_tr, ys_tr, Xs_te, ys_te, s_meta)
    uni = UnifierMLP.symbolist(Xs_tr, ys_tr, Xs_te, ys_te, s_meta)
    results["tasks"]["symbolist"] = {
        "description": "Recover XOR(bit0,bit1) among 6 binary features",
        "specialized": spec,
        "unifier": uni,
        "winner": _winner(spec["primary"], uni["primary"], "maximize"),
        "metric": "holdout_accuracy (+ rule_recovered)",
    }
    print(f"  specialized: {spec}")
    print(f"  unifier:     {uni}")

    # ---- 2 Connectionist ----
    print("== Connectionist ==")
    Xc_tr, yc_tr, Xc_te, yc_te, c_meta = make_connectionist(seed=SEED)
    spec = specialized_connectionist(Xc_tr, yc_tr, Xc_te, yc_te, c_meta)
    uni = UnifierMLP.connectionist(Xc_tr, yc_tr, Xc_te, yc_te, c_meta)
    results["tasks"]["connectionist"] = {
        "description": "Two moons nonlinear binary classification",
        "specialized": spec,
        "unifier": uni,
        "winner": _winner(spec["primary"], uni["primary"], "maximize"),
        "metric": "holdout_accuracy",
    }
    print(f"  specialized: {spec}")
    print(f"  unifier:     {uni}")

    # ---- 3 Evolutionary ----
    print("== Evolutionary ==")
    e_meta = make_evolutionary(dim=2, eval_budget=200, seed=SEED)
    spec = specialized_evolutionary(e_meta)
    uni = UnifierMLP.evolutionary(e_meta)
    results["tasks"]["evolutionary"] = {
        "description": "Minimize 2D Rastrigin, eval_budget=200",
        "specialized": spec,
        "unifier": uni,
        "winner": _winner(spec["primary"], uni["primary"], "minimize"),
        "metric": "best_fitness (rastrigin, lower better)",
    }
    print(f"  specialized: {spec}")
    print(f"  unifier:     {uni}")

    # ---- 4 Bayesian ----
    print("== Bayesian ==")
    flips, b_meta = make_bayesian(n_obs=12, true_theta=0.72, seed=SEED)
    spec = specialized_bayesian(flips, b_meta)
    uni = UnifierMLP.bayesian(flips, b_meta)
    results["tasks"]["bayesian"] = {
        "description": "Infer coin bias theta=0.72 from 12 flips; Beta(1,1) prior",
        "meta": {
            "heads": b_meta["heads"],
            "n_obs": b_meta["n_obs"],
            "true_theta": b_meta["true_theta"],
            "true_posterior_mean": b_meta["true_posterior_mean"],
        },
        "specialized": spec,
        "unifier": uni,
        "winner": _winner(spec["primary"], uni["primary"], "minimize"),
        "metric": "KL(true_posterior || estimate) lower better; also CI coverage",
    }
    print(f"  specialized: {spec}")
    print(f"  unifier:     {uni}")

    # ---- 5 Analogizer ----
    print("== Analogizer ==")
    Xa_s, ya_s, Xa_q, ya_q, a_meta = make_analogizer(n_shots=5, seed=SEED)
    spec = specialized_analogizer(Xa_s, ya_s, Xa_q, ya_q, a_meta)
    uni = UnifierMLP.analogizer(Xa_s, ya_s, Xa_q, ya_q, a_meta)
    results["tasks"]["analogizer"] = {
        "description": "3-class 2D Gaussians, 5-shot support, 1-NN vs MLP",
        "specialized": spec,
        "unifier": uni,
        "winner": _winner(spec["primary"], uni["primary"], "maximize"),
        "metric": "query accuracy",
    }
    print(f"  specialized: {spec}")
    print(f"  unifier:     {uni}")

    elapsed = time.time() - t0
    results["elapsed_seconds"] = round(elapsed, 2)

    # Summary table rows
    rows = []
    for tribe, t in results["tasks"].items():
        rows.append(
            {
                "tribe": tribe,
                "specialized_learner": t["specialized"]["learner"],
                "specialized_primary": t["specialized"]["primary"],
                "unifier_primary": t["unifier"]["primary"],
                "winner": t["winner"],
                "metric": t["metric"],
            }
        )
    results["summary_table"] = rows

    n_spec = sum(1 for r in rows if r["winner"] == "specialized")
    n_uni = sum(1 for r in rows if r["winner"] == "unifier")
    n_tie = sum(1 for r in rows if r["winner"] == "tie")
    results["scoreboard"] = {
        "specialized_wins": n_spec,
        "unifier_wins": n_uni,
        "ties": n_tie,
    }

    RESULTS_PATH.write_text(json.dumps(results, indent=2))
    print(f"\nWrote {RESULTS_PATH} in {elapsed:.1f}s")
    print("Scoreboard:", results["scoreboard"])
    return results


if __name__ == "__main__":
    run_all()
