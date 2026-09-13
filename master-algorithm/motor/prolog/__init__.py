"""Prolog-shaped archive + critic (SWI-Prolog preferred; Horn fallback)."""

from .bridge import PrologArchive, PrologCritic, swipl_available

__all__ = ["PrologArchive", "PrologCritic", "swipl_available"]
