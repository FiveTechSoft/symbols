"""Specialized tribe winners + one unification candidate (tiny MLP FA)."""
from __future__ import annotations

import numpy as np
from scipy import stats
from sklearn.neural_network import MLPClassifier, MLPRegressor
from sklearn.tree import DecisionTreeClassifier
from sklearn.linear_model import LogisticRegression
from sklearn.neighbors import KNeighborsClassifier
from sklearn.metrics import accuracy_score

from tasks import rastrigin, symbolist_rule_recovery


# ===========================================================================
# SPECIALIZED WINNERS
# ===========================================================================

def specialized_symbolist(X_train, y_train, X_test, y_test, meta):
    clf = DecisionTreeClassifier(max_depth=4, random_state=0)
    clf.fit(X_train, y_train)
    pred = clf.predict(X_test)
    acc = float(accuracy_score(y_test, pred))
    recovered = symbolist_rule_recovery(clf.predict, n_bits=meta["n_bits"])
    return {
        "learner": "DecisionTree",
        "holdout_accuracy": acc,
        "rule_recovered": bool(recovered),
        "primary": acc,
    }


def specialized_connectionist(X_train, y_train, X_test, y_test, meta):
    # Small MLP is the specialized connectionist winner; also report linear baseline
    mlp = MLPClassifier(
        hidden_layer_sizes=(16, 16),
        activation="relu",
        max_iter=800,
        random_state=0,
        learning_rate_init=0.01,
        early_stopping=False,
    )
    mlp.fit(X_train, y_train)
    mlp_acc = float(accuracy_score(y_test, mlp.predict(X_test)))

    lin = LogisticRegression(max_iter=500, random_state=0)
    lin.fit(X_train, y_train)
    lin_acc = float(accuracy_score(y_test, lin.predict(X_test)))

    return {
        "learner": "MLP(16,16)",
        "holdout_accuracy": mlp_acc,
        "linear_baseline_accuracy": lin_acc,
        "primary": mlp_acc,
    }


def specialized_evolutionary(meta):
    """(1+lambda)-ES minimizing Rastrigin."""
    rng = np.random.default_rng(meta["seed"])
    dim = meta["dim"]
    lo, hi = meta["bounds"]
    budget = meta["eval_budget"]
    lam = 8
    sigma = 0.5

    x = rng.uniform(lo, hi, size=dim)
    best_fit = rastrigin(x)
    evals = 1
    history = [best_fit]

    while evals < budget:
        for _ in range(lam):
            if evals >= budget:
                break
            child = np.clip(x + sigma * rng.normal(size=dim), lo, hi)
            f = rastrigin(child)
            evals += 1
            history.append(min(best_fit, f) if f >= best_fit else f)
            if f < best_fit:
                best_fit = f
                x = child
                history[-1] = best_fit
        # anneal step size slightly
        sigma *= 0.97

    return {
        "learner": "(1+lambda)-ES",
        "best_fitness": float(best_fit),  # lower is better (rastrigin)
        "evals": evals,
        "primary": float(best_fit),
        "primary_sense": "minimize",
    }


def specialized_bayesian(flips, meta):
    """Exact Beta-Binomial conjugate update."""
    n = meta["n_obs"]
    heads = meta["heads"]
    alpha = 1 + heads
    beta = 1 + (n - heads)
    mean = alpha / (alpha + beta)
    # 90% central credible interval
    lo = float(stats.beta.ppf(0.05, alpha, beta))
    hi = float(stats.beta.ppf(0.95, alpha, beta))
    covers = lo <= meta["true_theta"] <= hi
    # KL(true_post || estimated) = 0 for exact
    true_a = meta["true_posterior"]["alpha"]
    true_b = meta["true_posterior"]["beta"]
    kl = float(
        stats.entropy(
            [true_a / (true_a + true_b), true_b / (true_a + true_b)],
            [mean, 1 - mean],
        )
    )  # crude 2-bin KL on mean masses — better: analytic Beta KL
    kl_beta = _beta_kl(true_a, true_b, alpha, beta)
    mae = abs(mean - meta["true_theta"])
    return {
        "learner": "ConjugateBetaBinomial",
        "posterior_mean": float(mean),
        "ci90": [lo, hi],
        "covers_true_theta": bool(covers),
        "kl_to_true_posterior": float(kl_beta),
        "mae_vs_true_theta": float(mae),
        "primary": float(kl_beta),
        "primary_sense": "minimize",
    }


def _beta_kl(a0, b0, a1, b1):
    """KL(Beta(a0,b0) || Beta(a1,b1))."""
    # Use digamma via scipy
    from scipy.special import digamma, betaln

    return float(
        betaln(a1, b1)
        - betaln(a0, b0)
        + (a0 - a1) * digamma(a0)
        + (b0 - b1) * digamma(b0)
        + (a1 - a0 + b1 - b0) * digamma(a0 + b0)
    )


def specialized_analogizer(X_s, y_s, X_q, y_q, meta):
    knn = KNeighborsClassifier(n_neighbors=1)
    knn.fit(X_s, y_s)
    acc = float(accuracy_score(y_q, knn.predict(X_q)))
    return {
        "learner": "1-NN",
        "holdout_accuracy": acc,
        "n_shots": meta["n_shots"],
        "primary": acc,
    }


# ===========================================================================
# UNIFICATION CANDIDATE: tiny MLP as general function approximator
# ===========================================================================
# Rationale: Domingos highlights connectionist nets as a plausible substrate
# for unification. We force ONE algorithm class (sklearn MLP) onto all five
# tasks via task-specific encodings / surrogates — no tribe-specific models.

class UnifierMLP:
    """Same MLP class used everywhere; wrappers differ only in encoding."""

    name = "UnifierMLP"

    @staticmethod
    def symbolist(X_train, y_train, X_test, y_test, meta):
        clf = MLPClassifier(
            hidden_layer_sizes=(32, 16),
            activation="relu",
            max_iter=600,
            random_state=1,
            learning_rate_init=0.01,
        )
        clf.fit(X_train, y_train)
        pred = clf.predict(X_test)
        acc = float(accuracy_score(y_test, pred))
        recovered = symbolist_rule_recovery(clf.predict, n_bits=meta["n_bits"])
        return {
            "learner": UnifierMLP.name,
            "holdout_accuracy": acc,
            "rule_recovered": bool(recovered),
            "primary": acc,
        }

    @staticmethod
    def connectionist(X_train, y_train, X_test, y_test, meta):
        # Same architecture class as specialized MLP but fixed "unifier" hyperparams
        clf = MLPClassifier(
            hidden_layer_sizes=(32, 16),
            activation="relu",
            max_iter=600,
            random_state=1,
            learning_rate_init=0.01,
        )
        clf.fit(X_train, y_train)
        acc = float(accuracy_score(y_test, clf.predict(X_test)))
        return {
            "learner": UnifierMLP.name,
            "holdout_accuracy": acc,
            "primary": acc,
        }

    @staticmethod
    def evolutionary(meta):
        """Surrogate-assisted search: sample → fit MLPRegressor → optimize surrogate."""
        rng = np.random.default_rng(meta["seed"] + 7)
        dim = meta["dim"]
        lo, hi = meta["bounds"]
        budget = meta["eval_budget"]

        # Phase 1: initial DOE (~40% of budget)
        n_init = max(20, budget // 5)
        X = rng.uniform(lo, hi, size=(n_init, dim))
        y = np.array([rastrigin(xi) for xi in X])
        evals = n_init
        best_fit = float(y.min())
        best_x = X[y.argmin()].copy()

        remaining = budget - evals
        # Iteratively refit surrogate and propose candidates
        while evals < budget:
            # Fit MLP on z-scored inputs predicting fitness
            mu, sd = X.mean(axis=0), X.std(axis=0) + 1e-8
            Xs = (X - mu) / sd
            reg = MLPRegressor(
                hidden_layer_sizes=(32, 16),
                activation="relu",
                max_iter=400,
                random_state=1,
                learning_rate_init=0.01,
            )
            reg.fit(Xs, y)

            # Propose many candidates, pick lowest predicted fitness
            n_cand = 200
            cand = rng.uniform(lo, hi, size=(n_cand, dim))
            pred = reg.predict((cand - mu) / sd)
            # Also inject some random exploration
            order = np.argsort(pred)
            batch = min(8, budget - evals)
            chosen = []
            for idx in order:
                chosen.append(cand[idx])
                if len(chosen) >= batch - 1:
                    break
            # one pure random explore
            chosen.append(rng.uniform(lo, hi, size=dim))
            for xi in chosen:
                if evals >= budget:
                    break
                fi = rastrigin(xi)
                evals += 1
                X = np.vstack([X, xi])
                y = np.append(y, fi)
                if fi < best_fit:
                    best_fit = float(fi)
                    best_x = np.asarray(xi).copy()

        return {
            "learner": UnifierMLP.name,
            "best_fitness": float(best_fit),
            "evals": evals,
            "primary": float(best_fit),
            "primary_sense": "minimize",
            "note": "MLPRegressor surrogate + propose-eval loop",
        }

    @staticmethod
    def bayesian(flips, meta):
        """Train MLP on synthetic Beta-Binomial rollouts to map (heads,n)->theta.
        This is a learned ammortized estimator — not a true posterior.
        Uncertainty via deep ensemble of MLPs for a crude CI.
        """
        rng = np.random.default_rng(99)
        # Synthetic training: random (n, heads, true_theta) under uniform prior
        n_syn = 2000
        n_vals = rng.integers(5, 30, size=n_syn)
        thetas = rng.uniform(0.05, 0.95, size=n_syn)
        heads = rng.binomial(n_vals, thetas)
        # Features: [heads, n, heads/n]
        Feat = np.column_stack([heads, n_vals, heads / np.maximum(n_vals, 1)])
        # Target: true theta (point estimate task) — weak Bayesian
        targets = thetas

        models = []
        for i in range(5):
            reg = MLPRegressor(
                hidden_layer_sizes=(32, 16),
                activation="relu",
                max_iter=300,
                random_state=10 + i,
                learning_rate_init=0.01,
            )
            # bootstrap
            idx = rng.choice(n_syn, size=n_syn, replace=True)
            reg.fit(Feat[idx], targets[idx])
            models.append(reg)

        n = meta["n_obs"]
        h = meta["heads"]
        x = np.array([[h, n, h / n]], dtype=float)
        preds = np.array([m.predict(x)[0] for m in models])
        mean = float(np.clip(preds.mean(), 1e-3, 1 - 1e-3))
        # Crude CI from ensemble spread
        lo = float(np.clip(np.percentile(preds, 5), 0, 1))
        hi = float(np.clip(np.percentile(preds, 95), 0, 1))
        # Widen if ensemble collapsed
        if hi - lo < 0.05:
            lo = max(0.0, mean - 0.15)
            hi = min(1.0, mean + 0.15)
        covers = lo <= meta["true_theta"] <= hi

        # Approximate posterior as Beta matched to mean with vague concentration
        # Match mean with concentration ~ n+2 like conjugate, then KL
        conc = n + 2
        a_hat = mean * conc
        b_hat = (1 - mean) * conc
        true_a = meta["true_posterior"]["alpha"]
        true_b = meta["true_posterior"]["beta"]
        kl = _beta_kl(true_a, true_b, a_hat, b_hat)
        mae = abs(mean - meta["true_theta"])

        return {
            "learner": UnifierMLP.name,
            "posterior_mean": mean,
            "ci90": [lo, hi],
            "covers_true_theta": bool(covers),
            "kl_to_true_posterior": float(kl),
            "mae_vs_true_theta": float(mae),
            "primary": float(kl),
            "primary_sense": "minimize",
            "note": "amortized MLP ensemble; Beta-matched for KL",
        }

    @staticmethod
    def analogizer(X_s, y_s, X_q, y_q, meta):
        clf = MLPClassifier(
            hidden_layer_sizes=(32, 16),
            activation="relu",
            max_iter=600,
            random_state=1,
            learning_rate_init=0.01,
        )
        clf.fit(X_s, y_s)
        acc = float(accuracy_score(y_q, clf.predict(X_q)))
        return {
            "learner": UnifierMLP.name,
            "holdout_accuracy": acc,
            "n_shots": meta["n_shots"],
            "primary": acc,
        }
