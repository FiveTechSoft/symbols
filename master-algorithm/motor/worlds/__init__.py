"""World plugins for the autodidactic motor."""

from .base import FamilySpec, Conjecture, VerifiedFact, WorldBase
from .sequences import SequencesWorld
from .logic import LogicWorld
from .geometry_world import GeometryWorld
from .symmetry import SymmetryWorld
from .chance import ChanceWorld
from .info import InfoWorld
from .astro import AstroWorld
from .algebra import AlgebraWorld
from .calculus import CalculusWorld
from .nets import NetsWorld
from .electro import ElectroWorld
from .physics import PhysicsWorld
from .chem import ChemWorld
from .loop import LoopWorld


def build_worlds():
    """Instantiate worlds sharing the kernel (sequences/logic/geometry + sciences)."""
    return {
        "sequences": SequencesWorld(),
        "logic": LogicWorld(),
        "geometry": GeometryWorld(),
        "symmetry": SymmetryWorld(),
        "chance": ChanceWorld(),
        "info": InfoWorld(),
        "astro": AstroWorld(),
        "algebra": AlgebraWorld(),
        "calculus": CalculusWorld(),
        "nets": NetsWorld(),
        "electro": ElectroWorld(),
        "physics": PhysicsWorld(),
        "chem": ChemWorld(),
        "loop": LoopWorld(),
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
    "AstroWorld",
    "AlgebraWorld",
    "CalculusWorld",
    "NetsWorld",
    "ElectroWorld",
    "PhysicsWorld",
    "ChemWorld",
    "LoopWorld",
    "build_worlds",
]
