import argparse
import glob
import json
import os
import sys

import numpy as np

if __package__:
    from . import load_dataset
else:
    import load_dataset


def _quantiles(values):
    values = np.asarray(values)
    if values.size == 0:
        return None
    quantile_values = np.quantile(
        values.astype(np.float64), [0, 0.25, 0.5, 0.75, 0.9, 0.95, 0.99, 1]
    )
    return {
        "min": float(quantile_values[0]),
        "p25": float(quantile_values[1]),
        "p50": float(quantile_values[2]),
        "p75": float(quantile_values[3]),
        "p90": float(quantile_values[4]),
        "p95": float(quantile_values[5]),
        "p99": float(quantile_values[6]),
        "max": float(quantile_values[7]),
        "mean": float(np.mean(values.astype(np.float64))),
    }


def _target_comparison(prediction, target):
    prediction = np.asarray(prediction, dtype=np.float64)
    target = np.asarray(target, dtype=np.float64)
    if prediction.size == 0:
        return {"agreement_rate": None, "residual": None}
    residual = prediction - target
    return {
        "agreement_rate": float(np.mean(np.sign(prediction) == np.sign(target))),
        "residual": {
            "mean": float(np.mean(residual)),
            "mae": float(np.mean(np.abs(residual))),
            "rmse": float(np.sqrt(np.mean(residual ** 2))),
            "quantiles": _quantiles(residual),
        },
    }


def _board_keys(dataset):
    exact = []
    symmetric = []
    for features, stm in zip(dataset["X"], dataset["stm"]):
        canonical = load_dataset.canonical_position_key(
            features[0], features[1], stm, dataset["bs"]
        )
        black, white = (
            (features[0], features[1]) if stm == 1 else (features[1], features[0])
        )
        board = np.where(black, 1, np.where(white, 2, 0)).astype(np.uint8)
        exact.append(bytes((dataset["bs"], int(stm))) + board.tobytes(order="C"))
        symmetric.append(canonical)
    return exact, symmetric


def _result_counts(results):
    return {
        "wins": int(np.count_nonzero(results == 1)),
        "draws": int(np.count_nonzero(results == 0)),
        "losses": int(np.count_nonzero(results == -1)),
    }


def _game_results(dataset):
    games = {}
    black_results = dataset["final"].astype(np.int32) * np.where(dataset["stm"] == 1, 1, -1)
    for game_id, result in zip(dataset["game_id"], black_results):
        games[int(game_id)] = int(result)
    black = np.fromiter(games.values(), dtype=np.int8, count=len(games))
    return {
        "black": _result_counts(black),
        "white": _result_counts(-black),
    }


def _raw_score_bins(dataset, bins=10):
    if len(dataset["raw_search"]) == 0:
        return []
    stm_sign = np.where(dataset["stm"] == 1, 1.0, -1.0)
    raw_black = dataset["raw_search"].astype(np.float64) * stm_sign
    result_black = dataset["final"].astype(np.float64) * stm_sign
    order = np.argsort(raw_black, kind="stable")
    groups = np.array_split(order, min(bins, len(order)))
    output = []
    for group in groups:
        scores = raw_black[group]
        results = result_black[group]
        output.append({
            "raw_score_min": float(np.min(scores)),
            "raw_score_max": float(np.max(scores)),
            "samples": int(len(group)),
            "mean_raw_score": float(np.mean(scores)),
            "mean_final_black": float(np.mean(results)),
            "black_win_rate": float(np.mean(results == 1)),
            "draw_rate": float(np.mean(results == 0)),
            "black_loss_rate": float(np.mean(results == -1)),
        })
    return output


def _split_report(dataset):
    exact_keys, symmetric_keys = _board_keys(dataset)
    exact_unique = set(exact_keys)
    symmetric_unique = set(symmetric_keys)
    sample_count = len(dataset["X"])
    game_ids = set(int(value) for value in dataset["game_id"])
    opening_ids = set(int(value) for value in dataset["opening_id"])
    games = len(game_ids)
    outcomes = _game_results(dataset)
    ply_values, ply_counts = np.unique(dataset["ply"], return_counts=True)
    normalized_search = dataset["search"]
    normalized_td = dataset["td"]

    return {
        "samples": int(sample_count),
        "unique_games": games,
        "unique_openings": len(opening_ids),
        "results_by_color_per_game": outcomes,
        "ply_distribution": {
            str(int(ply)): int(count) for ply, count in zip(ply_values, ply_counts)
        },
        "ply_quantiles": _quantiles(dataset["ply"]),
        "depth_quantiles": _quantiles(dataset["depth"]),
        "nodes_quantiles": _quantiles(dataset["nodes"]),
        "raw_search_quantiles": _quantiles(dataset["raw_search"]),
        "played_score_quantiles": _quantiles(dataset["played_score"]),
        "saturation_rates": {
            "search_abs_normalized_at_least_0_99": (
                float(np.mean(np.abs(normalized_search) >= 0.99)) if sample_count else None
            ),
            "td_abs_normalized_at_least_0_99": (
                float(np.mean(np.abs(normalized_td) >= 0.99)) if sample_count else None
            ),
        },
        "target_agreement_and_residuals": {
            "search_vs_final": _target_comparison(normalized_search, dataset["final"]),
            "td_vs_final": _target_comparison(normalized_td, dataset["final"]),
            "search_vs_td": _target_comparison(normalized_search, normalized_td),
        },
        "raw_score_calibration": {
            "description": "Descriptive empirical raw-score bins; no recalibration applied.",
            "score_perspective": "black",
            "bins": _raw_score_bins(dataset),
        },
        "duplicates": {
            "exact_duplicate_positions": sample_count - len(exact_unique),
            "exact_duplicate_rate": (
                float((sample_count - len(exact_unique)) / sample_count) if sample_count else None
            ),
            "symmetry_duplicate_positions": sample_count - len(symmetric_unique),
            "symmetry_duplicate_rate": (
                float((sample_count - len(symmetric_unique)) / sample_count)
                if sample_count else None
            ),
        },
        "_game_ids": game_ids,
        "_exact_positions": exact_unique,
        "_symmetric_positions": symmetric_unique,
    }


def analyze_dataset(prefix, validation_prefix=None):
    """
    Analyze dataset quality and optional validation-split overlap.
    @param prefix ; str ; training dataset prefix or shard path
    @param validation_prefix ; str or None ; optional validation prefix or shard path
    @return dict ; JSON-compatible report
    """
    train_data = load_dataset.load_dataset(prefix)
    train_report = _split_report(train_data)

    validation_data = None
    if validation_prefix is not None:
        validation_data = load_dataset.load_dataset(validation_prefix)
        if len(validation_data["X"]) == 0:
            raise ValueError("the explicitly requested validation dataset is empty")
        if (
            validation_data["bs"] != train_data["bs"]
            or validation_data["scale"] != train_data["scale"]
        ):
            raise ValueError("training and validation dataset metadata does not match")
    else:
        candidate = str(prefix) + "_val"
        if os.path.isfile(candidate) or glob.glob(candidate + "-*.bin"):
            validation_prefix = candidate
            validation_data = load_dataset.load_dataset(validation_prefix)
            if len(validation_data["X"]) == 0:
                validation_data = None
            elif (
                validation_data["bs"] != train_data["bs"]
                or validation_data["scale"] != train_data["scale"]
            ):
                raise ValueError("training and validation dataset metadata does not match")

    overlap = None
    validation_report = None
    if validation_data is not None:
        validation_report = _split_report(validation_data)
        game_overlap = train_report["_game_ids"] & validation_report["_game_ids"]
        if game_overlap:
            sample = min(game_overlap)
            raise ValueError(
                f"game_id {sample} occurs in both training and validation datasets"
            )
        exact_overlap = train_report["_exact_positions"] & validation_report["_exact_positions"]
        symmetry_overlap = (
            train_report["_symmetric_positions"] & validation_report["_symmetric_positions"]
        )
        train_exact_count = len(train_report["_exact_positions"])
        train_symmetric_count = len(train_report["_symmetric_positions"])
        overlap = {
            "overlapping_game_ids": 0,
            "exact_position_overlap": len(exact_overlap),
            "exact_position_overlap_rate_of_train_unique": (
                len(exact_overlap) / train_exact_count if train_exact_count else None
            ),
            "symmetry_position_overlap": len(symmetry_overlap),
            "symmetry_position_overlap_rate_of_train_unique": (
                len(symmetry_overlap) / train_symmetric_count
                if train_symmetric_count else None
            ),
        }

    for report in (train_report, validation_report):
        if report is not None:
            del report["_game_ids"]
            del report["_exact_positions"]
            del report["_symmetric_positions"]

    return {
        "format_version": load_dataset.VERSION,
        "board_size": int(train_data["bs"]),
        "score_scale": int(train_data["scale"]),
        "train": train_report,
        "validation": validation_report,
        "train_validation_overlap": overlap,
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description="Analyze an NNUE dataset shard set.")
    parser.add_argument("prefix", help="training dataset prefix or shard file")
    parser.add_argument(
        "--validation", help="optional validation dataset prefix or shard file"
    )
    parser.add_argument("--output", help="optional path to write the JSON report")
    args = parser.parse_args(argv)
    try:
        report = analyze_dataset(args.prefix, args.validation)
        rendered = json.dumps(report, indent=2, sort_keys=True) + "\n"
        if args.output:
            with open(args.output, "w", encoding="utf-8") as output_file:
                output_file.write(rendered)
        sys.stdout.write(rendered)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
