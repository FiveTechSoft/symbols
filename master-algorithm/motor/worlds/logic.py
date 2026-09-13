"""
Discrete logic world: learn parity / conjunction / XOR-from-examples.

Critic = Prolog `holds_bit_fn(Kind)` over `holds_bit/3` examples.
Dead-end family: claim the target is a constant-true function.
"""

from __future__ import annotations

import itertools
import random
from typing import Any

from .base import Conjecture, FamilySpec, VerifiedFact, WorldBase


def _gen_examples(target: str, n_bits: int, seed: int = 42) -> list[tuple[list[int], int]]:
    rng = random.Random(seed)
    examples = []
    # Exhaust small space for n_bits<=3; sample otherwise
    space = list(itertools.product([0, 1], repeat=n_bits))
    if len(space) > 16:
        space = rng.sample(space, 16)
    for bits in space:
        bits = list(bits)
        if target == "parity":
            out = 0
            for b in bits:
                out ^= b
        elif target == "and_all":
            out = 0 if 0 in bits else 1
        elif target == "xor2":
            out = bits[0] ^ bits[1] if len(bits) >= 2 else bits[0]
        else:
            out = 1
        examples.append((bits, out))
    return examples


class LogicWorld(WorldBase):
    name = "logic"

    def __init__(self, n_bits: int = 3) -> None:
        self.n_bits = n_bits
        # Three hidden targets the engine must rediscover from examples
        self.targets = {
            "parity": _gen_examples("parity", n_bits),
            "and_all": _gen_examples("and_all", n_bits, seed=43),
            "xor2": _gen_examples("xor2", n_bits, seed=44),
        }
        self._active_target = "parity"  # rotates via param / family
        self._critic = None
        self._archive = None

    def bind(self, archive, critic) -> None:
        self._archive = archive
        self._critic = critic

    def families(self) -> dict[str, FamilySpec]:
        return {
            "boolean_from_examples": FamilySpec(
                id="boolean_from_examples",
                description="Hypothesize parity/and_all/xor2/const against example tables; param=target index",
                dead_end=False,
                param=0,  # 0=parity,1=and,2=xor2
                param_max=2,
                unlocked=True,
                unlock_order=0,
            ),
            "dead_end_const_true": FamilySpec(
                id="dead_end_const_true",
                description="DEAD END: claim every target is const(1)",
                dead_end=True,
                param=0,
                param_max=0,
                unlocked=True,
                unlock_order=1,
            ),
        }

    def observe(self) -> dict[str, Any]:
        return {
            "n_bits": self.n_bits,
            "targets": list(self.targets.keys()),
            "n_examples": {k: len(v) for k, v in self.targets.items()},
        }

    def _target_name(self, param: int) -> str:
        return ["parity", "and_all", "xor2"][param]

    def hypothesize(self, family: FamilySpec, archive_confirmed: dict, step: int) -> list[Conjecture]:
        out: list[Conjecture] = []
        if family.id == "boolean_from_examples":
            target = self._target_name(family.param)
            # Candidate hypothesis language (human-designed catalog)
            candidates = ["parity", "and_all", "xor2", "const(0)", "const(1)"]
            for kind in candidates:
                out.append(
                    Conjecture(
                        name=f"bitfn_{target}_is_{kind.replace('(', '_').replace(')', '')}",
                        family=family.id,
                        world=self.name,
                        formula=f"examples[{target}] ⊨ {kind}",
                        payload={"kind": "bit_fn", "target": target, "hyp": kind},
                        relation_type="bit_fn",
                    )
                )
        elif family.id == "dead_end_const_true":
            for target in self.targets:
                out.append(
                    Conjecture(
                        name=f"NEG_{target}_const1",
                        family=family.id,
                        world=self.name,
                        formula=f"examples[{target}] ⊨ const(1)",
                        payload={"kind": "bit_fn", "target": target, "hyp": "const(1)"},
                        relation_type="dead_end:const",
                    )
                )
        return out

    def verify(self, conjecture: Conjecture) -> VerifiedFact:
        p = conjecture.payload
        target = p["target"]
        hyp = p["hyp"]
        examples = self.targets[target]
        # Load examples into Prolog layer via critic
        ok, cex = self._critic.check_bit_fn(hyp, examples)
        if ok and self._archive and not hyp.startswith("const"):
            # Only archive productive (non-dead-end const) matches as bit_fn
            if conjecture.family != "dead_end_const_true":
                self._archive.assert_bit_fn(conjecture.name, target, hyp)
                for i, (bits, out) in enumerate(examples[:4]):
                    # keep theory small: sample
                    self._archive.assert_holds_bit(i + 1000 * hash(target) % 10000, bits, out)
        return VerifiedFact(
            name=conjecture.name,
            family=conjecture.family,
            world=self.name,
            formula=conjecture.formula,
            true=ok,
            support=f"Prolog holds_bit_fn({hyp}) on |ex|={len(examples)}" if ok else "finite fail",
            counterexample=cex,
            relation_type=conjecture.relation_type,
        )

    def known_bit_fns(self) -> list[tuple[str, str, str]]:
        """Expose archived bit_fn/3 for cross-world COMPARE transfer."""
        if self._archive is None:
            return []
        if hasattr(self._archive, "list_bit_fns"):
            return self._archive.list_bit_fns()
        return []

    def transfer_prior(self, archive_confirmed: dict) -> list[Conjecture]:
        """Priors are the verified bit_fn matches — siblings may reuse them."""
        # Logic does not re-hypothesize here; exposure is via known_bit_fns / archive.
        return []
