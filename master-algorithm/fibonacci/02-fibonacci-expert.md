# Fibonacci sequence-alone expert? — experiment report

**Hypothesis (Anto):** If we gave an AI the Fibonacci sequence to learn everything possible about it, searching all kinds of relations and connections, could it learn until it becomes an expert system?

**This experiment does not claim the miner “understands” Fibonacci.** It measures rediscovery of *checkable* relations inside a fixed hypothesis language, from a numeric prefix alone.

## Method

**Input:** only the prefix \(F_0,\ldots,F_N\) with \(F_0=0\), \(F_1=1\), for \(N \in \{20,40,80\}\).

**Fixed hypothesis language (H1–H8):**

| ID | Search |
|----|--------|
| H1 | Linear recurrences order 1–3 with small integer coeffs in \([-5,5]\) (exact hold on the prefix; min \(L_1\) among fits) |
| H2 | Consecutive ratios \(F(n+1)/F(n)\) vs \(\varphi=(1+\sqrt{5})/2\) |
| H3 | Cassini: \(F(n+1)F(n-1)-F(n)^2=(-1)^n\); Catalan \(r=2\): \(F(n)^2-F(n+2)F(n-2)=(-1)^{n-2}F(2)^2\) (F₀=0 indexing) |
| H4 | \(\gcd(F(a),F(b))=F(\gcd(a,b))\) on many pairs \(a,b\le N\) |
| H5 | Pisano periods \(\pi(m)\) for \(m=2..20\) (period must be observable in the prefix) |
| H6 | Zeckendorf: greedy non-consecutive Fib sums for integers \(1..\min(F_N,8000)\) |
| H7 | Binet: \(F(n)=\mathrm{round}(\varphi^n/\sqrt{5})\) on the prefix |
| H8 | **Negative controls (must reject):** \(F(n)=2F(n-1)\); \(F(n)\) always prime; ratios \(\to e\); ratios \(\to\pi\) |

**Output:** growing knowledge base entries `{name, formula, support, counterexample|none, discovered_at_N}` in `results.json`.

**Implementation:** `miner.py` (numpy; Decimal for Binet at large \(n\)). Seed `42` documented; searches are deterministic.

## Table: true relations vs \(N\)

| N | Core internal true (plateau count) | Pisano moduli confirmed (m=2..20) | Negatives correctly rejected | F_N | Runtime |
|---|--------------------------------------|-----------------------------------|------------------------------|-----|---------|
| 20 | **10** | 7 / 19 | 4 / 4 | 6765 | ~0.02 s |
| 40 | **10** | 15 / 19 | 4 / 4 | 102334155 | ~0.04 s |
| 80 | **10** | 19 / 19 | 4 / 4 | 23416728348467685 | ~0.07 s |

**Core true names (identical at all three N):**
`classic_fibonacci_recurrence`, `linear_recurrence_order_2` \(F(n)=F(n-1)+F(n-2)\), `linear_recurrence_order_3` (padded same law), `ratio_limit_phi`, `cassini_identity`, `catalan_identity_r2`, `gcd_identity`, `pisano_periods_m2_to_20` (summary), `zeckendorf`, `binet_formula`.

**Shape:** clear **plateau** in core relation count after \(N=20\). Extra terms do **not** unlock new *kinds* of internal facts in this language. The only growth is **Pisano coverage** (longer prefix → longer periods become observable): 7 → 15 → 19 moduli. That is diminishing-returns specialization, not a new theory of Fibonacci.

## Gold-list coverage

| Fact | Mark | First found at N |
|------|------|------------------|
| \(F(n)=F(n-1)+F(n-2)\) | **rediscovered** | 20 |
| \(F(n+1)/F(n)\to\varphi\) | **rediscovered** | 20 |
| Cassini identity | **rediscovered** | 20 |
| Catalan identity (r=2) | **rediscovered** | 20 |
| \(\gcd(F(a),F(b))=F(\gcd(a,b))\) | **rediscovered** | 20 |
| Pisano periodicity mod m | **rediscovered** | 20 |
| Zeckendorf representations | **rediscovered** | 20 |
| Binet formula | **rediscovered** | 20 |
| Phyllotaxis / plant spirals | **needs-external-data** | — |
| Art / architecture aesthetics | **needs-external-data** | — |
| Naming / Liber Abaci history | **needs-external-data** | — |
| Deep entry-point / Wall–Sun–Sun-style facts | **not-in-search-space** | — |
| Tiling / combinatorial interpretation | **not-in-search-space** | — |

All negatives were rejected at every N (0 false accepts).

## Verdict

Sequence-alone search **can** become a narrow “expert” at *internal, checkable* Fibonacci mathematics **if and only if** those facts live in the hypothesis language being searched: the miner rediscovered the recurrence, φ-ratios, Cassini/Catalan, the gcd identity, Pisano periods, Zeckendorf, and Binet by \(N=20\), then **plateaued**—more terms mainly filled in longer Pisano periods, matching the theory that once \(F(n)=F(n-1)+F(n-2)\) is known, extra sequence data is mostly redundant. It **cannot** become a general Fibonacci expert from the sequence alone: phyllotaxis, art, and history need external grounding, and combinatorial or deep number-theoretic stories need languages (tilings, entry points, …) that were never searched. Expertise here is **hypothesis language + search + verification**, not “read more Fibonacci numbers until understanding emerges.” The miner rediscovers relations; it does not understand Fibonacci.
