import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path

import numpy as np

from nnue.generate_dataset.src import analyze_dataset
from nnue.generate_dataset.tests.test_load_dataset import write_shard


def sample(features=None, stm=1, **values):
    features = np.zeros((2, 25), dtype=np.uint8) if features is None else features
    result = {
        "features": features,
        "stm": stm,
        "ply": int(np.asarray(features).sum()),
        "policy_moves": [next(
            index for index in range(25)
            if not np.asarray(features).reshape(2, 25)[:, index].any()
        )],
        "played_move": next(
            index for index in range(25)
            if not np.asarray(features).reshape(2, 25)[:, index].any()
        ),
    }
    result.update(values)
    return result


class AnalyzeDatasetTests(unittest.TestCase):
    def test_reports_sample_and_per_game_metrics(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "train"
            first = np.zeros((2, 25), dtype=np.uint8)
            second = np.zeros((2, 25), dtype=np.uint8)
            second[1, 0] = 1
            write_shard(Path(f"{prefix}-00000.bin"), samples=[
                sample(first, stm=1, game_id=10, opening_id=3, final=10000,
                       search=9999, td=9000, raw_search=500.0, depth=2, nodes=100),
                sample(second, stm=0, game_id=10, opening_id=3, final=-10000,
                       search=-5000, td=-7500, raw_search=-200.0, depth=4, nodes=900),
                sample(first, stm=1, game_id=11, opening_id=4, final=0,
                       search=0, td=100, raw_search=0.0, depth=3, nodes=400,
                       end_reason=2),
            ])

            report = analyze_dataset.analyze_dataset(str(prefix))

            train = report["train"]
            self.assertEqual(report["format_version"], 3)
            self.assertEqual(train["samples"], 3)
            self.assertEqual(train["unique_games"], 2)
            self.assertEqual(train["unique_openings"], 2)
            self.assertEqual(train["results_by_color_per_game"]["black"]["wins"], 1)
            self.assertEqual(train["results_by_color_per_game"]["black"]["draws"], 1)
            self.assertEqual(train["results_by_color_per_game"]["white"]["losses"], 1)
            self.assertEqual(train["ply_distribution"], {"0": 2, "1": 1})
            self.assertEqual(train["depth_quantiles"]["min"], 2.0)
            self.assertEqual(train["nodes_quantiles"]["max"], 900.0)
            self.assertAlmostEqual(
                train["saturation_rates"]["search_abs_normalized_at_least_0_99"],
                1 / 3,
            )
            self.assertEqual(
                train["target_agreement_and_residuals"]["search_vs_final"]["agreement_rate"],
                1.0,
            )
            self.assertIsNotNone(
                train["target_agreement_and_residuals"]["td_vs_final"]["residual"]["mae"]
            )
            self.assertEqual(len(train["raw_score_calibration"]["bins"]), 3)
            self.assertEqual(report["validation"], None)
            self.assertEqual(report["train_validation_overlap"], None)

    def test_reports_exact_and_symmetry_duplicates_and_split_overlap(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "train"
            validation_prefix = Path(directory) / "train_val"
            first = np.zeros((2, 25), dtype=np.uint8)
            first[1, 1] = 1
            first[1, 8] = 1
            rotated = np.rot90(first.reshape(2, 5, 5), 2, axes=(1, 2)).reshape(2, 25)
            validation_rotation = np.rot90(
                first.reshape(2, 5, 5), 1, axes=(1, 2)
            ).reshape(2, 25)
            write_shard(Path(f"{prefix}-00000.bin"), samples=[
                sample(first, stm=1, game_id=1),
                sample(rotated, stm=1, game_id=2),
                sample(first.copy(), stm=1, game_id=3),
            ])
            write_shard(Path(f"{validation_prefix}-00000.bin"), samples=[
                sample(validation_rotation, stm=1, game_id=4),
            ])

            report = analyze_dataset.analyze_dataset(
                str(prefix), str(validation_prefix)
            )

            self.assertEqual(report["train"]["duplicates"]["exact_duplicate_positions"], 1)
            self.assertEqual(report["train"]["duplicates"]["symmetry_duplicate_positions"], 2)
            overlap = report["train_validation_overlap"]
            self.assertEqual(overlap["exact_position_overlap"], 0)
            self.assertEqual(overlap["symmetry_position_overlap"], 1)
            self.assertEqual(overlap["overlapping_game_ids"], 0)

    def test_rejects_game_ids_crossing_splits_and_explicit_empty_validation(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "train"
            validation_prefix = Path(directory) / "validation"
            write_shard(Path(f"{prefix}-00000.bin"), samples=[sample(game_id=44)])
            write_shard(
                Path(f"{validation_prefix}-00000.bin"),
                samples=[sample(game_id=44)],
            )
            with self.assertRaisesRegex(ValueError, "occurs in both"):
                analyze_dataset.analyze_dataset(str(prefix), str(validation_prefix))

            empty_prefix = Path(directory) / "empty"
            write_shard(Path(f"{empty_prefix}-00000.bin"), samples=[])
            with self.assertRaisesRegex(ValueError, "validation dataset is empty"):
                analyze_dataset.analyze_dataset(str(prefix), str(empty_prefix))

    def test_rejects_incompatible_split_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "train"
            validation_prefix = Path(directory) / "validation"
            write_shard(Path(f"{prefix}-00000.bin"), samples=[sample()])
            write_shard(
                Path(f"{validation_prefix}-00000.bin"),
                board_size=7,
                samples=[{}],
            )
            with self.assertRaisesRegex(ValueError, "metadata does not match"):
                analyze_dataset.analyze_dataset(str(prefix), str(validation_prefix))

    def test_auto_missing_validation_is_graceful_but_invalid_files_are_not_hidden(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "train"
            write_shard(Path(f"{prefix}-00000.bin"), samples=[sample()])
            self.assertIsNone(analyze_dataset.analyze_dataset(str(prefix))["validation"])

            write_shard(Path(f"{prefix}_val-00000.bin"), version=2, samples=[sample()])
            with self.assertRaisesRegex(ValueError, "unsupported dataset version"):
                analyze_dataset.analyze_dataset(str(prefix))

    def test_cli_writes_and_prints_json(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "train"
            output_path = Path(directory) / "report.json"
            write_shard(Path(f"{prefix}-00000.bin"), samples=[sample()])
            stdout = io.StringIO()
            with contextlib.redirect_stdout(stdout):
                result = analyze_dataset.main([
                    str(prefix),
                    "--output",
                    str(output_path),
                ])
            self.assertEqual(result, 0)
            self.assertEqual(json.loads(stdout.getvalue()), json.loads(output_path.read_text()))


if __name__ == "__main__":
    unittest.main()
