import struct
import tempfile
import unittest
from pathlib import Path

import numpy as np

from nnue.generate_dataset.src import load_dataset


def write_shard(
    path,
    board_size=5,
    version=3,
    feature_len=None,
    score_scale=10000,
    max_policy=8,
    samples=None,
):
    squares = board_size * board_size
    if feature_len is None:
        feature_len = 2 * squares
    if samples is None:
        samples = [{}]
    header = struct.pack(
        "<IIHHIHHHHQQQ",
        load_dataset.MAGIC,
        version,
        board_size,
        0,
        len(samples),
        feature_len,
        score_scale,
        max_policy,
        0,
        0,
        0,
        0,
    )
    records = bytearray()
    for spec in samples:
        features = np.zeros((2, squares), dtype=np.uint8)
        if "features" in spec:
            features = np.asarray(spec["features"], dtype=np.uint8).reshape(2, squares)
        own, opponent = features.reshape(2, board_size, board_size)
        stm = spec.get("stm", 1)
        ply = spec.get("ply", int(features.sum()))
        policy_count = spec.get("policy_count", 1)
        policy_moves = list(spec.get("policy_moves", [0]))
        policy_probs = list(spec.get("policy_probs", [65535]))
        policy_moves.extend([0] * (8 - len(policy_moves)))
        policy_probs.extend([0] * (8 - len(policy_probs)))
        position_key = spec.get(
            "position_key",
            load_dataset.fnv1a64(
                load_dataset.canonical_position_key(own, opponent, stm, board_size)
            ),
        )
        records.extend(features.tobytes())
        records.extend(struct.pack(
            "<BHhhhB",
            stm,
            ply,
            spec.get("search", 2500),
            spec.get("final", 10000),
            spec.get("td", -1250),
            policy_count,
        ))
        records.extend(b"".join(
            struct.pack("<HH", policy_moves[index], policy_probs[index])
            for index in range(8)
        ))
        records.extend(struct.pack(
            "<QQQffQHHHBb",
            spec.get("game_id", 1),
            spec.get("opening_id", 7),
            position_key,
            spec.get("raw_search", 120.5),
            spec.get("played_score", 110.0),
            spec.get("nodes", 1234),
            spec.get("depth", 4),
            spec.get("played_move", 0),
            spec.get("flags", load_dataset.SEARCH_VALID),
            spec.get("end_reason", 1),
            spec.get("tactical", 0),
        ))
    Path(path).write_bytes(header + records)


class LoadShardTests(unittest.TestCase):
    def test_loads_valid_v3_shard_and_exposes_quality_fields(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "dataset-00000.bin"
            features = np.zeros((2, 25), dtype=np.uint8)
            features[0, 12] = 1
            write_shard(path, samples=[{
                "features": features,
                "stm": 0,
                "search": 2500,
                "final": -10000,
                "td": 500,
                "policy_moves": [0],
                "played_move": 0,
                "raw_search": 42.25,
            }])

            data = load_dataset.load_shard(path)

            self.assertEqual(data["X"].shape, (1, 2, 5, 5))
            self.assertEqual(int(data["X"][0, 0, 2, 2]), 1)
            self.assertEqual(float(data["search"][0]), 0.25)
            self.assertEqual(float(data["final"][0]), -1.0)
            self.assertEqual(int(data["policy_move"][0, 0]), 0)
            self.assertEqual(float(data["raw_search"][0]), 42.25)
            self.assertEqual(int(data["depth"][0]), 4)
            self.assertEqual(int(data["played_move"][0]), 0)
            self.assertEqual(int(data["game_id"][0]), 1)
            self.assertEqual(int(data["position_key"][0]), int(
                load_dataset.fnv1a64(load_dataset.canonical_position_key(
                    features[0].reshape(5, 5), features[1].reshape(5, 5), 0, 5
                ))
            ))
            self.assertEqual(data["bs"], 5)
            self.assertEqual(data["scale"], 10000)

    def test_rejects_v2_and_other_unsupported_versions(self):
        for version in (2, 4):
            with self.subTest(version=version), tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "bad-version.bin"
                write_shard(path, version=version)
                with self.assertRaisesRegex(ValueError, "unsupported dataset version"):
                    load_dataset.load_shard(path)

    def test_rejects_unsupported_score_scale_and_policy_capacity(self):
        for options, message in (
            ({"score_scale": 100}, "unsupported score scale"),
            ({"max_policy": 7}, "unsupported maximum policy count"),
        ):
            with self.subTest(options=options), tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "bad-header.bin"
                write_shard(path, **options)
                with self.assertRaisesRegex(ValueError, message):
                    load_dataset.load_shard(path)

    def test_record_size_is_feature_length_plus_90(self):
        self.assertEqual(load_dataset._record_dtype(50, 8).itemsize, 140)
        self.assertEqual(load_dataset.HEADER_DTYPE.itemsize, 48)

    def test_rejects_inconsistent_feature_length_and_truncated_payload(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad-features.bin"
            write_shard(path, feature_len=49)
            with self.assertRaisesRegex(ValueError, "feature length"):
                load_dataset.load_shard(path)

            write_shard(path)
            path.write_bytes(path.read_bytes()[:-1])
            with self.assertRaisesRegex(ValueError, "invalid shard size"):
                load_dataset.load_shard(path)

    def test_rejects_mixed_shard_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "dataset"
            write_shard(Path(f"{prefix}-00000.bin"), board_size=5)
            write_shard(Path(f"{prefix}-00001.bin"), board_size=7)
            with self.assertRaisesRegex(ValueError, "metadata does not match"):
                load_dataset.load_dataset(str(prefix))

    def test_rejects_invalid_record_contracts(self):
        cases = [
            ("overlap", {"features": [[1] + [0] * 24, [1] + [0] * 24], "stm": 0},
             "overlapping stones"),
            ("ply", {"ply": 2}, "ply does not match"),
            ("parity", {"stm": 0}, "ply parity"),
            ("raw-score", {"raw_search": float("inf")}, "must be finite"),
            ("search-range", {"search": 10001}, "search target"),
            ("td-range", {"td": -10001}, "td target"),
            ("final", {"final": 5000}, "final target"),
            ("depth", {"depth": 0}, "depth"),
            ("flags", {"flags": 0x21}, "unknown bits"),
            ("search-valid", {"flags": 0}, "SEARCH_VALID"),
            ("end-reason", {"end_reason": 0}, "end_reason"),
            ("end-result", {"end_reason": 2}, "does not match the final game result"),
            ("tactical", {"tactical": 2}, "tactical value"),
            ("position-key", {"position_key": 0}, "position_key"),
            ("played-outside", {"played_move": 25}, "played move index"),
            ("policy-outside", {"policy_moves": [25]}, "policy move index"),
            ("zero-policy", {"policy_probs": [0]}, "must be positive"),
            ("policy-sum", {"policy_probs": [100]}, "do not sum to 1"),
            ("duplicate-policy", {
                "policy_count": 2,
                "policy_moves": [0, 0],
                "policy_probs": [32768, 32767],
            }, "duplicate moves"),
        ]
        for name, spec, message in cases:
            with self.subTest(name=name), tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "invalid.bin"
                write_shard(path, samples=[spec])
                with self.assertRaisesRegex(ValueError, message):
                    load_dataset.load_shard(path)

    def test_rejects_occupied_policy_and_played_moves(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "occupied.bin"
            features = np.zeros((2, 25), dtype=np.uint8)
            features[0, 0] = 1
            write_shard(path, samples=[{
                "features": features,
                "stm": 0,
                "policy_moves": [0],
                "played_move": 1,
            }])
            with self.assertRaisesRegex(ValueError, "policy move is not an empty square"):
                load_dataset.load_shard(path)

            write_shard(path, samples=[{
                "features": features,
                "stm": 0,
                "policy_moves": [1],
                "played_move": 0,
            }])
            with self.assertRaisesRegex(ValueError, "played move is not an empty square"):
                load_dataset.load_shard(path)

    def test_validates_tactical_labels_from_the_board(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "tactical.bin"
            features = np.zeros((2, 25), dtype=np.uint8)
            features[0, :4] = 1
            write_shard(path, samples=[{
                "features": features,
                "stm": 1,
                "ply": 4,
                "tactical": 1,
                "played_move": 4,
                "policy_moves": [4],
            }])
            self.assertEqual(int(load_dataset.load_shard(path)["tactical"][0]), 1)

            write_shard(path, samples=[{"tactical": 1}])
            with self.assertRaisesRegex(ValueError, "requires an immediate win"):
                load_dataset.load_shard(path)

            size = 7
            features = np.zeros((2, size * size), dtype=np.uint8)
            features[1, 3 * size + 1:3 * size + 5] = 1
            write_shard(path, board_size=size, samples=[{
                "features": features,
                "stm": 1,
                "ply": 4,
                "tactical": -1,
                "played_move": 0,
                "policy_moves": [0],
            }])
            self.assertEqual(int(load_dataset.load_shard(path)["tactical"][0]), -1)

            write_shard(path, board_size=size, samples=[{"tactical": -1}])
            with self.assertRaisesRegex(ValueError, "requires at least two opponent wins"):
                load_dataset.load_shard(path)

    def test_checks_game_metadata_across_shards(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "dataset"
            write_shard(Path(f"{prefix}-00000.bin"), samples=[{"game_id": 99}])
            write_shard(Path(f"{prefix}-00001.bin"), samples=[{"game_id": 99, "opening_id": 8}])
            with self.assertRaisesRegex(ValueError, "game metadata is inconsistent"):
                load_dataset.load_dataset(str(prefix))

    def test_canonical_key_is_d4_invariant_and_keeps_side_to_move(self):
        own = np.zeros((5, 5), dtype=np.uint8)
        opponent = np.zeros((5, 5), dtype=np.uint8)
        own[0, 1] = own[2, 3] = 1
        opponent[4, 2] = 1
        expected = load_dataset.canonical_position_key(own, opponent, 1, 5)
        for symmetry in range(8):
            transformed_own = load_dataset._sym_plane(own, symmetry)
            transformed_opponent = load_dataset._sym_plane(opponent, symmetry)
            self.assertEqual(
                load_dataset.canonical_position_key(transformed_own, transformed_opponent, 1, 5),
                expected,
            )
        self.assertNotEqual(
            load_dataset.canonical_position_key(own, opponent, 0, 5),
            expected,
        )


if __name__ == "__main__":
    unittest.main()
