"""World plugin protocol: observe / hypothesize / verify / transfer_prior."""

from __future__ import annotations

from dataclasses import dataclass, field, asdict
from typing import Any, Optional


@dataclass
class FamilySpec:
    id: str
    description: str
    dead_end: bool = False
    param: int = 0
    param_max: int = 0
    unlocked: bool = True
    unlock_order: int = 0
    saturated: bool = False
    n_visits: int = 0
    total_reward: float = 0.0

    @property
    def avg_reward(self) -> float:
        if self.n_visits == 0:
            return 0.0
        return self.total_reward / self.n_visits

    def to_dict(self) -> dict:
        return asdict(self)

    @classmethod
    def from_dict(cls, d: dict) -> "FamilySpec":
        return cls(**{k: v for k, v in d.items() if k in cls.__dataclass_fields__})


@dataclass
class Conjecture:
    name: str
    family: str
    world: str
    formula: str
    payload: dict = field(default_factory=dict)  # world-specific hypothesis data
    relation_type: str = ""
    from_transfer: bool = False
    transfer_source: Optional[str] = None


@dataclass
class VerifiedFact:
    name: str
    family: str
    world: str
    formula: str
    true: bool
    support: str
    counterexample: Optional[str]
    relation_type: str
    step: int = -1
    from_transfer: bool = False
    transfer_source: Optional[str] = None
    lemma_cited: Optional[str] = None  # geometry lemma reuse

    def to_dict(self) -> dict:
        return asdict(self)

    @classmethod
    def from_dict(cls, d: dict) -> "VerifiedFact":
        keys = cls.__dataclass_fields__
        return cls(**{k: v for k, v in d.items() if k in keys})


class WorldBase:
    """Plugin interface. Subclasses implement hypothesize + verify."""

    name: str = "base"

    def families(self) -> dict[str, FamilySpec]:
        raise NotImplementedError

    def observe(self) -> dict[str, Any]:
        """Return a short observation digest (for status / logs)."""
        return {}

    def hypothesize(
        self,
        family: FamilySpec,
        archive_confirmed: dict[str, VerifiedFact],
        step: int,
    ) -> list[Conjecture]:
        raise NotImplementedError

    def verify(self, conjecture: Conjecture) -> VerifiedFact:
        raise NotImplementedError

    def transfer_prior(
        self,
        archive_confirmed: dict[str, VerifiedFact],
    ) -> list[Conjecture]:
        """Optional: turn verified facts from sibling domains into priors."""
        return []

    def expand_family(self, family: FamilySpec, new_truths: int) -> str:
        """Language growth inside a family. Default: autodidact-style bump."""
        if family.dead_end:
            family.saturated = True
            return "dead-end → saturate (curiosity tax)"
        if family.param < family.param_max:
            old = family.param
            bump = 5 if "modular" in family.id or "pisano" in family.id else 1
            family.param = min(family.param_max, family.param + bump)
            return f"expand param {old}→{family.param}"
        family.saturated = True
        return "param at max → saturate"

    def unlock_next(self, families: dict[str, FamilySpec]) -> Optional[str]:
        locked = sorted(
            [f for f in families.values() if not f.unlocked],
            key=lambda f: f.unlock_order,
        )
        if not locked:
            return None
        nxt = locked[0]
        nxt.unlocked = True
        return nxt.id
