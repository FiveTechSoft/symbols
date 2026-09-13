"""World plugins for the autodidactic motor."""

from .base import FamilySpec, Conjecture, VerifiedFact, WorldBase
from .sequences import SequencesWorld
from .logic import LogicWorld
from .geometry_world import GeometryWorld


def build_worlds():
    """Instantiate the three (or more) worlds sharing the kernel."""
    return {
        "sequences": SequencesWorld(),
        "logic": LogicWorld(),
        "geometry": GeometryWorld(),
    }


__all__ = [
    "FamilySpec",
    "Conjecture",
    "VerifiedFact",
    "WorldBase",
    "SequencesWorld",
    "LogicWorld",
    "GeometryWorld",
    "build_worlds",
]
