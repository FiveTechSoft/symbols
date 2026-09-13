# Master Algorithm — First Experimental Battery

Tiny CPU experiments inspired by Pedro Domingos's *The Master Algorithm*:
can **one** learner class compete across the five ML tribes, or do specialized
algorithms still own their home turf?

## Setup

```bash
cd /workspace/master-algorithm
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
```

## Run

```bash
python experiments/run_battery.py
# or:
.venv/bin/python experiments/run_battery.py
```

Writes numeric results to `experiments/results.json`.
Interpretation: `01-first-battery.md`.

## Layout

```
experiments/
  tasks.py          # five tribe micro-tasks
  learners.py       # specialized winners + UnifierMLP
  run_battery.py    # entry point (seeds, metrics, JSON)
  results.json      # last run
01-first-battery.md # table + interpretation
```

## Unifier choice

**UnifierMLP** — one sklearn MLP class (`MLPClassifier` / `MLPRegressor`,
hidden `(32, 16)`) forced onto every task via encodings / surrogate search /
amortized inference. Chosen because connectionist nets are Domingos's
most-cited unification substrate; we test that claim honestly on tiny tasks.

## Seeds

Global seed `0` (also task-local seeds). Runtime target: under a few minutes on CPU.
