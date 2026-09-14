"""Scratch proof on archive-emerge/: clone ~0, new lever pays. No live merge of clones."""
import json
from pathlib import Path
from motor.kernel import MotorKernel
from motor.worlds.base import VerifiedFact, FamilySpec
from motor.understand import known_form_families, analyze, print_report

ROOT = Path(__file__).resolve().parents[1]
SCRATCH = ROOT / "archive-emerge"
theory = SCRATCH / "theory.pl"
assert theory.exists(), theory

k = MotorKernel(reset=False, archive_dir=SCRATCH)
k._known_form_families = known_form_families(theory)
fam = FamilySpec(id="bilinear_schema", description="x", param=4, param_max=4)

_, d_clone = k._score(
    [
        VerifiedFact(
            name="bilin_fib_r4", family="bilinear_schema_r4_g8", world="sequences",
            formula="same", true=True, support="t", counterexample=None, relation_type="b",
        ),
        VerifiedFact(
            name="bilin_fib_r5", family="bilinear_schema_r4_g9", world="sequences",
            formula="same", true=True, support="t", counterexample=None, relation_type="b",
        ),
    ],
    fam,
    ["new_true", "new_true"],
)

k._known_form_families = {f for f in k._known_form_families if f != "bit_circuit"}
_, d_new = k._score(
    [
        VerifiedFact(
            name="bitfn_and_series", family="boolean_from_examples", world="logic",
            formula="AND=series", true=True, support="t", counterexample=None, relation_type="bit",
        )
    ],
    fam,
    ["new_true"],
)

k._known_form_families.discard("conserv_delta0")
_, d_xfer = k._score(
    [
        VerifiedFact(
            name="transfer_conserv_chem_to_loop", family="loop_pred_scan", world="loop",
            formula="d0->dx", true=True, support="t", counterexample=None, relation_type="c",
            from_transfer=True,
        )
    ],
    fam,
    ["new_true"],
)

_, d_carve = k._score(
    [
        VerifiedFact(
            name="false_xor_as_and", family="boolean_from_examples", world="logic",
            formula="neq", true=False, support="", counterexample="form-family mismatch",
            relation_type="bit",
        )
    ],
    fam,
    ["new_reject"],
)

k2 = MotorKernel(reset=False, archive_dir=SCRATCH)
latest = k2.tick(steps=2)

live = analyze(ROOT / "archive" / "theory.pl")
# merge policy: never merge clones/off-path; only new lever families
merge = []
for fid, meta in analyze(theory).families.items():
    if meta["tag"] == "lever" and meta["necessary"] and fid not in live.necessary_ids:
        merge.append(fid)

proof = {
    "clone_score": d_clone["score"],
    "new_lever_score": d_new["score"],
    "transfer_score": d_xfer["score"],
    "carve_score": d_carve["score"],
    "detail_clone": d_clone,
    "detail_new": d_new,
    "detail_xfer": d_xfer,
    "assertions": {
        "clone_near_zero": d_clone["score"] < 0.2,
        "new_lever_large": d_new["score"] >= 2.0,
        "transfer_largest": d_xfer["score"] >= d_new["score"],
        "carve_positive": d_carve["score"] > 0.5,
    },
    "scratch_tick": {
        "steps": latest["ticks_this_run"],
        "n_verified": latest["n_verified_facts"],
        "tail": [
            {
                kk: e.get("score_detail", {}).get(kk)
                for kk in (
                    "score",
                    "n_new_form_family",
                    "n_known_clone",
                    "n_offpath",
                    "new_true",
                    "form_families",
                )
            }
            for e in latest.get("step_log_tail", [])
        ],
    },
    "merge_clones": False,
    "merge_new_levers_only": merge,
}
out = ROOT / "runs" / "emerge-proof.json"
out.write_text(json.dumps(proof, indent=2))
print(json.dumps(proof["assertions"], indent=2))
print("scores clone/new/xfer/carve:", d_clone["score"], d_new["score"], d_xfer["score"], d_carve["score"])
print("PROOF_OK", all(proof["assertions"].values()))
print("--- LIVE ---")
print_report(ROOT / "archive" / "theory.pl")
