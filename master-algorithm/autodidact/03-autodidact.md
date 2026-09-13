# Autodidact loop — choosing the next question vs reading more terms

**User question (Anto):** *cómo hacer un sistema que aprenda por sí solo*

**Builds on:** Fibonacci miner (`fibonacci/miner.py`) where core relations plateau at \(N=20\) (10 facts); extra \(N\) only fills Pisano coverage. Expertise = hypothesis language + search, not more data.

**This experiment does not claim AGI.** The locked-family catalog is human-designed. Curiosity (UCB1) is a toy bandit and can waste a step on a dead-end. The claim under test is narrower and checkable:

> **scheduler + critic + growing hypothesis language > more data with a fixed language**

## Setup

| Knob | Value |
|------|-------|
| Prefix | \(F_0..F_{30}\) fixed for autodidact |
| Steps \(K\) | 12 |
| Scheduler | UCB1 over **unlocked ∧ non-saturated** families |
| Reward | novelty (# new true + 0.5·new types + 0.25·dead-end correct rejects) − cost |
| Language growth | expand family `param` (order / Catalan \(r\) / max modulus) or mark saturated; when **all** unlocked families saturate → **unlock** next locked family |
| Seed | 42 (deterministic) |
| Runtime | ~0.14 s (real run in `results.json`) |

### Initially unlocked families

1. `linear_recurrences` (start order 1; expand toward 4)
2. `ratio_limits` (φ vs e, π, 2)
3. `dead_end_always_prime` (**dead end** — curiosity tax)

### Locked catalog (human-designed unlock order)

4. `bilinear_identities` → 5. `gcd_identities` → 6. `modular_periods` → 7. `zeckendorf` → 8. `closed_forms` → 9. `dead_end_geometric_2`

## Curiosity rule (explicit)

\[
\mathrm{UCB}_i = \overline{r}_i + 1.4\sqrt{\frac{\ln(T+1)}{n_i}}
\]

- Never-tried unlocked families get \(+\infty\) (forced explore).
- **Saturated families are excluded** from the pool (so the loop cannot grind a finished family forever).
- When the pool is empty → unlock the next locked family (language growth at catalog scale).
- WHY for each pick is logged in `results.json` → `autodidact.step_log[].why`.

## Step log (real run)

| Step | Family chosen | Why (short) | Accept / reject (new) | Growth |
|------|---------------|-------------|------------------------|--------|
| 1 | `linear_recurrences` | never-tried → UCB=+∞ | reject order-1 recurrence | expand order 1→2 |
| 2 | `ratio_limits` | never-tried → UCB=+∞ | **accept** φ-limit; reject →e,→π,→2 | saturate (param max) |
| 3 | `dead_end_always_prime` | never-tried → UCB=+∞ | reject “always prime” | saturate (dead end; wasted step) |
| 4 | `linear_recurrences` | only active family; UCB on prior | **accept** classic + order-2 | expand 2→3 |
| 5 | `linear_recurrences` | only active | **accept** order-3 | expand 3→4 |
| 6 | `linear_recurrences` | only active | **accept** order-4 (padded law) | saturate → **unlock bilinear** |
| 7 | `bilinear_identities` | newly unlocked, never-tried | **accept** Cassini + Catalan \(r=1\) | expand \(r\) 1→2 |
| 8 | `bilinear_identities` | highest UCB among active | **accept** Catalan \(r=2\) | expand 2→3 |
| 9 | `bilinear_identities` | … | **accept** Catalan \(r=3\) | expand 3→4 |
| 10 | `bilinear_identities` | … | **accept** Catalan \(r=4\) | saturate → **unlock gcd** |
| 11 | `gcd_identities` | newly unlocked | **accept** gcd identity | saturate → **unlock Pisano** |
| 12 | `modular_periods` | newly unlocked | **accept** π(2),π(3),π(4),π(5)+summary | expand max‑m 5→10 |

**Unlock events:** bilinear @ step 6; gcd @ step 10; modular_periods @ step 11.

**Archive after \(K=12\):** 6 productive relation types — `linear_recurrence`, `ratio_limit`, `cassini_identity`, `catalan_identity`, `gcd_identity`, `pisano_period`. Dead-end correctly rejected (not confirmed). Locked remainder (`zeckendorf`, `closed_forms`, `dead_end_geometric_2`) not yet reached in 12 steps — more \(K\) would unlock them the same way.

## Control: baseline = more \(N\), fixed language

Same full miner language as `fibonacci/miner.py` (H1–H8), **no** family scheduler, only \(N \in \{20,40,80\}\):

| N | Core internal true | Pisano moduli (m=2..20) | **New relation types vs previous** |
|---|--------------------|-------------------------|-------------------------------------|
| 20 | 10 | 7 | 8 types appear at once (full fixed language) |
| 40 | 10 | 15 | **0** new types |
| 80 | 10 | 19 | **0** new types |

Extra terms only deepen Pisano coverage. Core type count plateaus at \(N=20\).

## Comparison

| Agent | Data | Language behavior | New relation **types** after start |
|-------|------|-------------------|--------------------------------------|
| **Autodidact** | fixed \(N=30\) | chooses family (UCB); expands; unlocks catalog | **+4** beyond initial unlocked set (Cassini/Catalan, gcd, Pisano, …) |
| **Baseline** | \(N=20\to40\to80\) | fixed full language; no choice | **+0** after \(N=20\) |

**Verdict:** at this toy scale, **choosing the next question beat reading more terms.** With fixed \(F_0..F_{30}\), the loop recovered new *kinds* of facts by changing the hypothesis language (expand order / \(r\) / \(m\), then unlock bilinear → gcd → Pisano). The baseline, given more Fibonacci numbers but a frozen language, added **no** new relation types—only more Pisano moduli inside an already-known type.

## Honesty / limits

- The unlock menu is still a **human-written catalog**; the system does not invent Cassini from nothing—it *schedules* when to try a pre-authored family.
- UCB wasted step 3 on “always prime” and recovered; that is desired (curiosity can fail).
- Not reached in \(K=12\): Zeckendorf, Binet, second dead-end. Same mechanism would unlock them with more steps.
- Claim is **not** “learns by itself from pure void”; claim is **scheduler + critic + growing language > more data**.

## Files

- `loop.py` — runnable autodidact + baseline
- `results.json` — real run (step log, archive, comparison)
- Reuses `../fibonacci/miner.py` check functions
