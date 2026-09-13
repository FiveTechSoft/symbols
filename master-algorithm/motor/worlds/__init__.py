"""World plugins for the autodidactic motor."""

from .base import FamilySpec, Conjecture, VerifiedFact, WorldBase
from .sequences import SequencesWorld
from .logic import LogicWorld
from .geometry_world import GeometryWorld
from .symmetry import SymmetryWorld
from .chance import ChanceWorld
from .info import InfoWorld


def build_worlds():
    """Instantiate worlds sharing the kernel (sequences/logic/geometry + sciences)."""
    return {
        "sequences": SequencesWorld(),
        "logic": LogicWorld(),
        "geometry": GeometryWorld(),
        "symmetry": SymmetryWorld(),
        "chance": ChanceWorld(),
        "info": InfoWorld(),
    }


__all__ = [
    "FamilySpec",
    "Conjecture",
    "VerifiedFact",
    "WorldBase",
    "SequencesWorld",
    "LogicWorld",
    "GeometryWorld",
    "SymmetryWorld",
    "ChanceWorld",
    "InfoWorld",
    "build_worlds",
]
