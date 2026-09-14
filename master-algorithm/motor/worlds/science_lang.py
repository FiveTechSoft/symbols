"""
Science hypothesis language — operators that molt/spawn forever.

Philosophy: there is no done. Saturation → molt/spawn a NEW schema class
(not in the seed list). Novelty bonus stays > 0. UCB arms are re-opened
after molt; a saturated family never permanently dies.

Skin only — never writes verified/1. Critic still gates every fact.
"""

from __future__ import annotations

import copy
from dataclasses import asdict, dataclass, field
from typing import Any, Optional

from .schema_lang import SchemaClass


# Seed operators (human-authored). Emergence = spawn_* NOT in this dict.
SCIENCE_SEED: dict[str, dict[str, Any]] = {
    # --- symmetry ---
    "sym_invariant_scan": {
        "description": "Scan discrete invariants (parity, mod-2 sum, involution)",
        "unlocked": True,
        "origin": "seed",
        "order": 2,  # bit-width / table order
        "order_max": 6,
    },
    "sym_group_table": {
        "description": "Hypothesize Z2 / Klein composition-table laws",
        "unlocked": True,
        "origin": "seed",
        "order": 2,
        "order_max": 4,
    },
    "sym_dead_product": {
        "description": "DEAD END: claim product conserved mod-2",
        "unlocked": True,
        "dead_end": True,
        "origin": "seed",
    },
    "sym_compare_transfer": {
        "description": "COMPARE: reuse verified bit_fn/invariant across systems",
        "unlocked": True,
        "origin": "seed",
    },
    # --- chance ---
    "chance_bayes_scan": {
        "description": "Tiny Bayes / log-odds identities on generated 2x2 tables",
        "unlocked": True,
        "origin": "seed",
        "order": 2,
        "order_max": 4,
    },
    "chance_entropy_scan": {
        "description": "Entropy inequalities on generated 2..k outcome dists",
        "unlocked": True,
        "origin": "seed",
        "order": 2,  # k outcomes
        "order_max": 6,
    },
    "chance_dead_negH": {
        "description": "DEAD END: claim H(p)<0 or H decreases under mixing",
        "unlocked": True,
        "dead_end": True,
        "origin": "seed",
    },
    "chance_extrapolate": {
        "description": "EXTRAPOLATE: re-test accepted identities on larger tables",
        "unlocked": False,
        "origin": "seed_locked",
        "order": 3,
        "order_max": 8,
    },
    # --- info ---
    "info_mi_scan": {
        "description": "Mutual information / independence on tiny joint tables",
        "unlocked": True,
        "origin": "seed",
        "order": 2,
        "order_max": 4,
    },
    "info_transfer_bitfn": {
        "description": "COMPARE: transfer archived bit_fn prior into MI structure",
        "unlocked": True,
        "origin": "seed",
    },
    "info_dead_mi0": {
        "description": "DEAD END: claim MI always 0 on every joint",
        "unlocked": True,
        "dead_end": True,
        "origin": "seed",
    },
    # --- astro ---
    "astro_period_scan": {
        "description": "Scan periods T vs semi-major a on generated circular orbits",
        "unlocked": True,
        "origin": "seed",
        "order": 3,
        "order_max": 8,
    },
    "astro_kepler_scan": {
        "description": "Scan T^2/a^3 constancy (Kepler-3 form) within eps",
        "unlocked": True,
        "origin": "seed",
        "order": 3,
        "order_max": 8,
    },
    "astro_angmom_scan": {
        "description": "Angular-momentum-like invariant on discrete 2-body circular table",
        "unlocked": True,
        "origin": "seed",
        "order": 3,
        "order_max": 6,
    },
    "astro_dead_linear": {
        "description": "DEAD END: claim T proportional to a",
        "unlocked": True,
        "dead_end": True,
        "origin": "seed",
    },
    # --- algebra ---
    "alg_poly_zn": {
        "description": "Polynomial identities on small Z/nZ ((x+y)^2 vs expand)",
        "unlocked": True,
        "origin": "seed",
        "order": 5,
        "order_max": 13,
    },
    "alg_linear_2x2": {
        "description": "Exact 2x2 linear systems over Q (integer dets)",
        "unlocked": True,
        "origin": "seed",
        "order": 2,
        "order_max": 6,
    },
    "alg_mat_assoc": {
        "description": "Associativity of 2x2 matrix mult mod p",
        "unlocked": True,
        "origin": "seed",
        "order": 3,
        "order_max": 7,
    },
    "alg_dead_binom": {
        "description": "DEAD END: claim (x+y)^2 = x^2+y^2",
        "unlocked": True,
        "dead_end": True,
        "origin": "seed",
    },
    # --- calculus ---
    "calc_fwd_diff": {
        "description": "Forward difference Delta on fib/lucas and simple polynomials",
        "unlocked": True,
        "origin": "seed",
        "order": 4,
        "order_max": 12,
    },
    "calc_ft_discrete": {
        "description": "Discrete FTC: Delta(sum f) = f on finite prefixes",
        "unlocked": True,
        "origin": "seed",
        "order": 4,
        "order_max": 12,
    },
    "calc_power_diff": {
        "description": "Delta(x^n) ~ n x^{n-1} on integer lattice",
        "unlocked": True,
        "origin": "seed",
        "order": 2,
        "order_max": 6,
    },
    "calc_transfer_rec": {
        "description": "TRANSFER: rec/2 prior for Delta structure on that sequence",
        "unlocked": True,
        "origin": "seed",
    },
    "calc_dead_zero": {
        "description": "DEAD END: claim Delta is always zero",
        "unlocked": True,
        "dead_end": True,
        "origin": "seed",
    },
    # --- nets (connectionist miniature) ---
    "nets_perceptron": {
        "description": "Tiny linear threshold on AND/OR tables",
        "unlocked": True,
        "origin": "seed",
        "order": 2,
        "order_max": 4,
    },
    "nets_xor_depth": {
        "description": "XOR: single linear threshold fail; 2-layer/composition may hold",
        "unlocked": True,
        "origin": "seed",
        "order": 2,
        "order_max": 4,
    },
    "nets_grad_step": {
        "description": "One gradient step on 2-weight squared-error toy",
        "unlocked": True,
        "origin": "seed",
        "order": 2,
        "order_max": 4,
    },
    "nets_chain_transfer": {
        "description": "TRANSFER: discrete chain rule Delta(f(g)) vs product of Deltas",
        "unlocked": True,
        "origin": "seed",
    },
    "nets_dead_any_eta": {
        "description": "DEAD END: claim loss always decreases for any eta",
        "unlocked": True,
        "dead_end": True,
        "origin": "seed",
    },
    # --- electro ---
    "electro_ohm": {
        "description": "Ohm V=IR on generated (V,I,R) triples",
        "unlocked": True,
        "origin": "seed",
        "order": 3,
        "order_max": 8,
    },
    "electro_kirchhoff": {
        "description": "Kirchhoff current sum=0 at a node (tiny 3-edge net)",
        "unlocked": True,
        "origin": "seed",
        "order": 3,
        "order_max": 6,
    },
    "electro_switch_transfer": {
        "description": "TRANSFER: AND/OR as series/parallel switches from logic bit_fn",
        "unlocked": True,
        "origin": "seed",
    },
    "electro_dead_noconserve": {
        "description": "DEAD END: claim current not conserved at node",
        "unlocked": True,
        "dead_end": True,
        "origin": "seed",
    },
    # --- physics ---
    "phys_collision": {
        "description": "1D elastic collision: momentum+energy on generated m,v",
        "unlocked": True,
        "origin": "seed",
        "order": 2,
        "order_max": 6,
    },
    "phys_action_reaction": {
        "description": "Action=reaction pair on generated force samples",
        "unlocked": True,
        "origin": "seed",
        "order": 2,
        "order_max": 5,
    },
    "phys_dead_nomom": {
        "description": "DEAD END: claim momentum not conserved in elastic 1D",
        "unlocked": True,
        "dead_end": True,
        "origin": "seed",
    },
    # --- chem ---
    "chem_atom_balance": {
        "description": "Atom-count conservation on generated tiny reaction tables",
        "unlocked": True,
        "origin": "seed",
        "order": 2,
        "order_max": 5,
    },
    "chem_equilibrium_K": {
        "description": "K=products/reactants on 2-species equilibrium toy",
        "unlocked": True,
        "origin": "seed",
        "order": 2,
        "order_max": 5,
    },
    "chem_dead_noatoms": {
        "description": "DEAD END: claim atoms not conserved in reaction",
        "unlocked": True,
        "dead_end": True,
        "origin": "seed",
    },
}


@dataclass
class ScienceLanguage:
    """Growing science operator set. Shared pattern with HypothesisLanguage."""

    schemas: dict[str, SchemaClass] = field(default_factory=dict)
    generation: int = 0
    molt_history: list[dict] = field(default_factory=list)
    world_tag: str = "science"  # +astro/algebra/calculus/nets/electro/physics/chem
    novelty_floor: float = 0.25  # NEVER let novelty die

    @classmethod
    def seed_for(cls, world_tag: str) -> "ScienceLanguage":
        prefix = {
            "symmetry": "sym_",
            "chance": "chance_",
            "info": "info_",
            "astro": "astro_",
            "algebra": "alg_",
            "calculus": "calc_",
            "nets": "nets_",
            "electro": "electro_",
            "physics": "phys_",
            "chem": "chem_",
        }[world_tag]
        schemas = {}
        for sid, spec in SCIENCE_SEED.items():
            if sid.startswith(prefix):
                schemas[sid] = SchemaClass(id=sid, **spec)
                # Eternal curiosity: productive seeds start with novelty > 0
                if not schemas[sid].dead_end:
                    schemas[sid].novelty_bonus = max(
                        schemas[sid].novelty_bonus, cls.novelty_floor
                    )
        return cls(schemas=schemas, generation=0, world_tag=world_tag)

    def n_schema_classes(self) -> int:
        return len(self.schemas)

    def unlocked_ids(self) -> list[str]:
        return sorted(s.id for s in self.schemas.values() if s.unlocked)

    def seed_ids(self) -> set[str]:
        return {sid for sid in SCIENCE_SEED if sid.startswith(
            {
                "symmetry": "sym_",
                "chance": "chance_",
                "info": "info_",
                "astro": "astro_",
                "algebra": "alg_",
                "calculus": "calc_",
                "nets": "nets_",
                "electro": "electro_",
                "physics": "phys_",
                "chem": "chem_",
            }[self.world_tag]
        )}

    def spawned_ids(self) -> list[str]:
        seed = self.seed_ids()
        return sorted(sid for sid in self.schemas if sid not in seed)

    def snapshot(self) -> dict:
        return {
            "world_tag": self.world_tag,
            "generation": self.generation,
            "n_schema_classes": self.n_schema_classes(),
            "unlocked": self.unlocked_ids(),
            "spawned": self.spawned_ids(),
            "schemas": {k: v.to_dict() for k, v in self.schemas.items()},
            "molt_history": list(self.molt_history),
            "novelty_floor": self.novelty_floor,
        }

    def clone(self) -> "ScienceLanguage":
        return ScienceLanguage(
            schemas={k: SchemaClass.from_dict(v.to_dict()) for k, v in self.schemas.items()},
            generation=self.generation,
            molt_history=copy.deepcopy(self.molt_history),
            world_tag=self.world_tag,
            novelty_floor=self.novelty_floor,
        )

    def ensure_novelty_alive(self) -> None:
        """Standing rule: novelty_bonus never reaches 0 on productive schemas."""
        for s in self.schemas.values():
            if s.unlocked and not s.dead_end:
                s.novelty_bonus = max(s.novelty_bonus, self.novelty_floor)

    def molt(self, reason: str) -> dict:
        """
        tick→compare→plateau→molt/spawn→forever.

        Saturated family does NOT die: we unsaturate + spawn a class that was
        not in SCIENCE_SEED. Finite --max-molts is only a run cap.
        """
        before = self.unlocked_ids()
        actions: list[str] = []
        seed = self.seed_ids()

        # 1) Unlock locked seed schemas
        locked = [s for s in self.schemas.values() if not s.unlocked]
        if locked:
            locked.sort(key=lambda s: (1 if s.dead_end else 0, s.id))
            nxt = locked[0]
            nxt.unlocked = True
            nxt.saturated = False
            nxt.origin = f"molt:unlocked_gen{self.generation + 1}"
            actions.append(f"unlock_schema {nxt.id}")

        # 2) Raise order / extrapolate capacity on productive schemas
        for s in self.schemas.values():
            if not s.unlocked or s.dead_end:
                continue
            if s.order < s.order_max:
                old = s.order
                s.order = min(s.order_max, s.order + 1)
                s.saturated = False
                s.ticks_no_new_type = 0
                actions.append(f"raise_order {s.id} {old}→{s.order}")
                break  # one raise per molt keeps pressure

        # 3) ALWAYS spawn a non-seed class when any productive schema is saturated
        #    or when no structural unlock/raise happened beyond novelty
        saturated = [
            s for s in self.schemas.values()
            if s.unlocked and not s.dead_end and s.saturated
        ]
        need_spawn = bool(saturated) or not any(
            a.startswith("unlock") or a.startswith("raise") for a in actions
        )
        # Force spawn at least every other generation so seed-only runs fail the metric
        if self.generation % 2 == 1:
            need_spawn = True

        if need_spawn:
            base = None
            for cand in ("sym_invariant_scan", "chance_entropy_scan", "info_mi_scan",
                         "sym_group_table", "chance_bayes_scan", "info_transfer_bitfn",
                         "astro_kepler_scan", "astro_period_scan", "alg_poly_zn",
                         "alg_linear_2x2", "calc_fwd_diff", "calc_ft_discrete",
                         "nets_perceptron", "nets_xor_depth", "nets_grad_step",
                         "electro_ohm", "phys_collision", "chem_atom_balance"):
                if cand in self.schemas and self.schemas[cand].unlocked:
                    base = self.schemas[cand]
                    break
            if base is None:
                # any productive unlocked
                prod = [s for s in self.schemas.values() if s.unlocked and not s.dead_end]
                base = prod[0] if prod else None
            if base is not None:
                new_id = f"{base.id}_n{base.order}_g{self.generation + 1}"
                # ensure not colliding with seed
                assert new_id not in SCIENCE_SEED
                if new_id not in self.schemas:
                    self.schemas[new_id] = SchemaClass(
                        id=new_id,
                        description=f"Emergent operator from {base.id} @ order={base.order}",
                        unlocked=True,
                        origin=f"molt:spawn_gen{self.generation + 1}",
                        order=base.order,
                        order_max=base.order_max,
                        novelty_bonus=max(1.0, self.novelty_floor),
                    )
                    actions.append(f"spawn_schema {new_id}")

        # 4) Novelty floor + bump (eternal curiosity)
        for s in self.schemas.values():
            if s.unlocked and not s.dead_end:
                s.novelty_bonus = max(self.novelty_floor, min(3.0, s.novelty_bonus + 0.35))
                s.saturated = False  # molt re-opens arms — never die
                s.ticks_no_new_type = 0
        actions.append(f"novelty_floor≥{self.novelty_floor}; arms reopened")

        self.generation += 1
        info = {
            "generation": self.generation,
            "world_tag": self.world_tag,
            "reason": reason,
            "actions": actions,
            "unlocked_before": before,
            "unlocked_after": self.unlocked_ids(),
            "spawned": self.spawned_ids(),
            "n_schema_classes": self.n_schema_classes(),
            "philosophy": "tick→compare/extrapolate→if plateau then molt/spawn→forever",
        }
        self.molt_history.append(info)
        return info
