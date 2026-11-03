from __future__ import annotations

import math

from .config import SprtConfig


def elo(score: float) -> float | str:
    if score <= 0:
        return "-infinity"
    if score >= 1:
        return "+infinity"
    return 400 * math.log10(score / (1 - score))


def expected_score(rating: float) -> float:
    return 1 / (1 + 10 ** (-rating / 400))


def constrained_probabilities(counts: list[int], mean: float) -> list[float]:
    """Fit five outcome probabilities with a fixed normalized pair-score mean."""
    total = sum(counts) + 5e-9
    empirical = [(count + 1e-9) / total for count in counts]
    differences = [i / 4 - mean for i in range(5)]
    low = -1 / (1 - mean) + 1e-12
    high = 1 / mean - 1e-12
    for _ in range(120):
        multiplier = (low + high) / 2
        derivative = sum(
            p * d / (1 + multiplier * d) for p, d in zip(empirical, differences)
        )
        if derivative > 0:
            low = multiplier
        else:
            high = multiplier
    multiplier = (low + high) / 2
    fitted = [p / (1 + multiplier * d) for p, d in zip(empirical, differences)]
    normalization = sum(fitted)
    return [p / normalization for p in fitted]


def sprt_result(counts: list[int], config: SprtConfig) -> dict[str, object]:
    low = math.log(config.beta / (1 - config.alpha))
    high = math.log((1 - config.beta) / config.alpha)
    llr = 0.0
    if sum(counts):
        null = constrained_probabilities(counts, expected_score(config.elo0))
        alternative = constrained_probabilities(counts, expected_score(config.elo1))
        llr = sum(n * math.log(q / p) for n, p, q in zip(counts, null, alternative))
    decision = "disabled" if not config.enabled else "inconclusive"
    if config.enabled and sum(counts) >= config.min_pairs:
        if llr <= low:
            decision = "accept_h0"
        elif llr >= high:
            decision = "accept_h1"
    return {
        "method": "pentanomial_generalized_likelihood_ratio",
        "decision": decision, "llr": llr, "lower_bound": low, "upper_bound": high,
        "elo0": config.elo0, "elo1": config.elo1,
        "alpha": config.alpha, "beta": config.beta, "min_pairs": config.min_pairs,
        "regularization_per_bin": 1e-9,
        "error_rates": "Nominal Wald bounds; composite-model and finite-sample approximation.",
    }


def summarize(pair_scores: list[tuple[float, float]], config: SprtConfig) -> dict[str, object]:
    counts = [0] * 5
    wins = draws = losses = 0
    for first, second in pair_scores:
        counts[round((first + second) * 2)] += 1
        for score in (first, second):
            wins += score == 1
            draws += score == 0.5
            losses += score == 0
    games = 2 * len(pair_scores)
    score = (wins + draws / 2) / games if games else None
    interval: list[float | str] | None = None
    if score is not None:
        n = len(pair_scores)
        # Union-bound spending makes the interval valid at a data-dependent stopping time.
        alpha_n = 0.05 / (n * (n + 1))
        radius = math.sqrt(math.log(2 / alpha_n) / (2 * n))
        interval = [elo(max(0, score - radius)), elo(min(1, score + radius))]
    return {
        "perspective": "candidate_minus_reference", "pairs": len(pair_scores),
        "games": games, "wins": wins, "draws": draws, "losses": losses,
        "score": score, "elo_delta": elo(score) if score is not None else None,
        "elo_95_interval": interval,
        "interval_method": "anytime_valid_paired_Hoeffding_alpha_spending",
        "interval_assumptions": (
            "Independent bounded opening-pair scores; inference concerns the selected "
            "opening distribution and settings, not a universal rating."
        ),
        "pentanomial_counts": counts, "sprt": sprt_result(counts, config),
    }
