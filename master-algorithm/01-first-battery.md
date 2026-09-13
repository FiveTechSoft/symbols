# First Master-Algorithm Battery — Results & Interpretation

**Seed:** 0  ·  **Elapsed:** 0.77s  ·  **Unifier:** `UnifierMLP`

## Unifier (what we chose and why)

Connectionist substrate as Domingos-style unification candidate: force one MLP algorithm class onto all five tribe tasks via encodings/surrogates/amortization — honest about where it loses.

Implementation: `sklearn MLPClassifier/MLPRegressor (32,16)` applied to all five tasks
(classification directly; Rastrigin via MLPRegressor surrogate propose–eval;
coin bias via amortized MLP ensemble trained on synthetic Beta–Binomial rollouts;
few-shot via fitting the same MLP on the support set).

## Result table

| Tribe | Specialized | Spec primary | Unifier | Uni primary | Winner |
|---|---|---|---|---|---|
| symbolist | DecisionTree | 1, rule=True | UnifierMLP | 1, rule=True | **tie** |
| connectionist | MLP(16,16) | 0.97 (linear=0.82) | UnifierMLP | 0.98 | **unifier** |
| evolutionary | (1+lambda)-ES | 1.391 (evals=200) | UnifierMLP | 1.995 (evals=200) | **specialized** |
| bayesian | ConjugateBetaBinomial | 0, cover=True, mean=0.571 | UnifierMLP | 0.0009697, cover=False, mean=0.566 | **specialized** |
| analogizer | 1-NN | 0.9889 | UnifierMLP | 1 | **unifier** |

Primary metrics: accuracy (↑) for symbolist / connectionist / analogizer;
Rastrigin best fitness (↓) for evolutionary; KL(true posterior ‖ estimate) (↓) for Bayesian.

**Scoreboard:** specialized 2 · unifier 2 · ties 1

### Per-tribe notes

- **Symbolist:** Both DecisionTree and UnifierMLP reached perfect holdout accuracy and recovered XOR(bit0,bit1). Tie — discrete rules are learnable by either a shallow tree or a small net on this tiny problem.
- **Connectionist:** Specialized MLP(16,16)=0.97 vs linear baseline 0.82; UnifierMLP=0.98. Unifier slightly ahead (larger fixed net). Both crush the linear baseline — this is MLP home turf.
- **Evolutionary:** (1+λ)-ES best fitness 1.3907 vs MLP-surrogate 1.9950 (budget 200). **Specialized wins.** Surrogate search is competitive but the ES exploits the landscape more efficiently at this budget.
- **Bayesian:** Conjugate Beta–Binomial has KL=0 and 90% CI covers true θ=0.72. Unifier amortized ensemble: KL≈0.0009697, CI cover=False. **Specialized wins.** Point estimates are close; calibrated uncertainty is not.
- **Analogizer:** 1-NN=0.9889, UnifierMLP=1.0000 (5-shot, 3 Gaussian clusters). Unifier edged out — clusters are still easy enough that a net fit on 15 points generalizes; not a strong win for either philosophy.

## Interpretation: implications for a Master Algorithm

A single MLP class was **competitive on three of five tribes** (tie or win on symbolist, connectionist, analogizer) but **lost clearly on evolutionary search and Bayesian inference** — exactly the tribes whose native formalisms (population search; conjugate uncertainty) are farthest from supervised function approximation.

This does **not** support a claim that "neural nets already are the Master Algorithm." It supports a weaker, useful claim: a connectionist substrate can absorb classification-style tribes with little ceremony, while black-box optimization and calibrated posterior inference still reward specialized algorithms (or at least specialized *wrappers* around the net). A true unifier would need first-class support for search and for uncertainty, not only for ∇L.

Honesty check: tasks are tiny, CPU-second scale, and engineered. Wins/losses could flip with harder XOR depth, higher-D Rastrigin, or Omniglot-scale few-shot. The qualitative split (nets ≈ classifiers; nets < ES & Bayes) is the signal to carry forward.

## What to try next

1. **Harder symbolist:** 3-literal conjunction or parity of 3 bits with more noise bits; add an explicit rule-extraction metric for the MLP (truth-table fidelity / surrogate decision tree).
2. **Stronger unifier for opt + Bayes:** replace the MLP surrogate with Bayesian optimization (GP) or an evolution strategy that *uses* the net as a policy; for Bayesian, use MC-dropout / deep ensemble predictive intervals and score continuous ranked probability, not Beta-matched KL.
3. **Cross-tribe transfer encoding:** one shared input schema (task-id embedding + padded feature vector) and a single MLP trained multi-task / meta-learned across all five, then evaluate zero-shot within-tribe generalization.

---
*Generated from `experiments/results.json` (elapsed 0.77s).*
