"""Five tribe micro-tasks for the Master Algorithm battery."""
from __future__ import annotations

import numpy as np
from sklearn.datasets import make_moons
from sklearn.model_selection import train_test_split


# ---------------------------------------------------------------------------
# 1. Symbolist: hidden Boolean rule (XOR of two bits among noise)
# ---------------------------------------------------------------------------
def make_symbolist(n_train: int = 200, n_test: int = 100, n_bits: int = 6, seed: int = 0):
    rng = np.random.default_rng(seed)
    X = rng.integers(0, 2, size=(n_train + n_test, n_bits)).astype(float)
    # Hidden rule: XOR of bit 0 and bit 1
    y = (X[:, 0].astype(int) ^ X[:, 1].astype(int)).astype(int)
    X_train, X_test = X[:n_train], X[n_train:]
    y_train, y_test = y[:n_train], y[n_train:]
    meta = {
        "rule": "XOR(bit0, bit1)",
        "relevant_bits": [0, 1],
        "n_bits": n_bits,
    }
    return X_train, y_train, X_test, y_test, meta


def symbolist_rule_recovery(predict_fn, n_bits: int = 6) -> bool:
    """Check whether predictions match XOR(bit0,bit1) for all 2^n patterns
    when noise bits vary — we test all combinations of bit0/bit1 with
    noise bits fixed to 0, and also a few random noise settings."""
    # Exhaustive on first 2 bits with noise=0
    ok = True
    for b0 in (0, 1):
        for b1 in (0, 1):
            x = np.zeros((1, n_bits), dtype=float)
            x[0, 0], x[0, 1] = b0, b1
            pred = int(predict_fn(x)[0])
            if pred != (b0 ^ b1):
                ok = False
    # Also check with random noise — rule should ignore them
    rng = np.random.default_rng(42)
    for _ in range(16):
        x = rng.integers(0, 2, size=(1, n_bits)).astype(float)
        b0, b1 = int(x[0, 0]), int(x[0, 1])
        pred = int(predict_fn(x)[0])
        if pred != (b0 ^ b1):
            ok = False
            break
    return ok


# ---------------------------------------------------------------------------
# 2. Connectionist: two moons nonlinear classification
# ---------------------------------------------------------------------------
def make_connectionist(n_train: int = 200, n_test: int = 100, seed: int = 0):
    X, y = make_moons(n_samples=n_train + n_test, noise=0.15, random_state=seed)
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=n_test, random_state=seed, stratify=y
    )
    return X_train, y_train, X_test, y_test, {"dataset": "two_moons", "noise": 0.15}


# ---------------------------------------------------------------------------
# 3. Evolutionary: Rastrigin black-box (2D)
# ---------------------------------------------------------------------------
def rastrigin(x: np.ndarray) -> float:
    """Standard Rastrigin; global min 0 at origin. We maximize -rastrigin."""
    x = np.asarray(x, dtype=float).ravel()
    A = 10.0
    return float(A * len(x) + np.sum(x**2 - A * np.cos(2 * np.pi * x)))


def make_evolutionary(dim: int = 2, bounds: float = 5.12, eval_budget: int = 200, seed: int = 0):
    meta = {
        "fn": "rastrigin",
        "dim": dim,
        "bounds": [-bounds, bounds],
        "eval_budget": eval_budget,
        "optimum_fitness": 0.0,  # minimize rastrigin
        "seed": seed,
    }
    return meta


# ---------------------------------------------------------------------------
# 4. Bayesian: biased coin — infer theta from few flips
# ---------------------------------------------------------------------------
def make_bayesian(n_obs: int = 12, true_theta: float = 0.72, seed: int = 0):
    rng = np.random.default_rng(seed)
    flips = rng.binomial(1, true_theta, size=n_obs)
    heads = int(flips.sum())
    # True posterior under Beta(1,1) prior = Beta(1+heads, 1+n-heads)
    alpha_post = 1 + heads
    beta_post = 1 + (n_obs - heads)
    meta = {
        "true_theta": true_theta,
        "n_obs": n_obs,
        "heads": heads,
        "flips": flips.tolist(),
        "prior": "Beta(1,1)",
        "true_posterior": {"alpha": alpha_post, "beta": beta_post},
        "true_posterior_mean": alpha_post / (alpha_post + beta_post),
    }
    return flips, meta


# ---------------------------------------------------------------------------
# 5. Analogizer: few-shot 2D cluster classification
# ---------------------------------------------------------------------------
def make_analogizer(
    n_classes: int = 3,
    n_shots: int = 5,
    n_test_per_class: int = 30,
    seed: int = 0,
):
    rng = np.random.default_rng(seed)
    # Well-separated Gaussian clusters
    centers = np.array([[0.0, 0.0], [3.0, 0.5], [1.0, 3.0]])[:n_classes]
    X_support, y_support = [], []
    X_query, y_query = [], []
    for c in range(n_classes):
        pts = rng.normal(loc=centers[c], scale=0.55, size=(n_shots + n_test_per_class, 2))
        X_support.append(pts[:n_shots])
        y_support.append(np.full(n_shots, c))
        X_query.append(pts[n_shots:])
        y_query.append(np.full(n_test_per_class, c))
    X_s = np.vstack(X_support)
    y_s = np.concatenate(y_support)
    X_q = np.vstack(X_query)
    y_q = np.concatenate(y_query)
    meta = {
        "n_classes": n_classes,
        "n_shots": n_shots,
        "centers": centers.tolist(),
    }
    return X_s, y_s, X_q, y_q, meta
