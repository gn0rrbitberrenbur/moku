import glob
import os

import numpy as np

HEADER_DTYPE = np.dtype([
    ("magic", "<u4"),
    ("version", "<u4"),
    ("board_size", "<u2"),
    ("rule", "<u2"),
    ("sample_count", "<u4"),
    ("feature_len", "<u2"),
    ("score_scale", "<u2"),
    ("max_policy", "<u2"),
    ("reserved0", "<u2"),
    ("reserved", "<u8", 3),
])

MAGIC = 0x314E4E47  # 'GNN1'
VERSION = 3
MIN_BOARD_SIZE = 5
MAX_BOARD_SIZE = 19
SCORE_SCALE = 10000
MAX_POLICY = 8
SEARCH_VALID = 1
KNOWN_FLAGS = 0x1F
POLICY_SUM_TOLERANCE = MAX_POLICY / 65535.0
FNV_OFFSET_BASIS = 14695981039346656037
FNV_PRIME = 1099511628211


def _record_dtype(feature_len, max_policy):
    """
    Build the packed sample record dtype for the C++ writer.
    @param feature_len ; int ; length of the feature planes (2 * squares)
    @param max_policy ; int ; fixed number of policy entries per sample
    @return np.dtype ; packed record dtype for one sample
    """
    policy = np.dtype([("move", "<u2"), ("prob", "<u2")])
    return np.dtype([
        ("feat", np.int8, int(feature_len)),
        ("stm", np.uint8),
        ("ply", "<u2"),
        ("search", "<i2"),
        ("final", "<i2"),
        ("td", "<i2"),
        ("pcount", np.uint8),
        ("policy", policy, int(max_policy)),
        ("game_id", "<u8"),
        ("opening_id", "<u8"),
        ("position_key", "<u8"),
        ("raw_search", "<f4"),
        ("played_score", "<f4"),
        ("nodes", "<u8"),
        ("depth", "<u2"),
        ("played_move", "<u2"),
        ("flags", "<u2"),
        ("end_reason", np.uint8),
        ("tactical", np.int8),
    ])


def canonical_position_key(own, opponent, stm, bs):
    """
    Return the absolute-color D4-canonical key for an STM-encoded board.
    @param own ; np.ndarray ; own-stone feature plane
    @param opponent ; np.ndarray ; opponent-stone feature plane
    @param stm ; int ; 1 for black to move, 0 for white to move
    @param bs ; int ; board size
    @return bytes ; canonical key including board size and side to move
    """
    black, white = (own, opponent) if int(stm) == 1 else (opponent, own)
    board = np.where(black, 1, np.where(white, 2, 0)).astype(np.uint8)
    transforms = []
    for rotation in range(4):
        rotated = np.rot90(board, rotation)
        transforms.append(rotated.tobytes(order="C"))
        transforms.append(np.fliplr(rotated).tobytes(order="C"))
    return bytes((bs, int(stm))) + min(transforms)


def fnv1a64(data):
    """
    Hash canonical key bytes using the generator's FNV-1a 64-bit convention.
    @param data ; bytes ; canonical key
    @return int ; unsigned 64-bit hash
    """
    value = FNV_OFFSET_BASIS
    for byte in data:
        value = ((value ^ byte) * FNV_PRIME) & 0xFFFFFFFFFFFFFFFF
    return value


def _wins_at(board, move, color, bs):
    row, col = divmod(int(move), bs)
    for dr, dc in ((0, 1), (1, 0), (1, 1), (1, -1)):
        count = 1
        for sign in (-1, 1):
            r, c = row + sign * dr, col + sign * dc
            while 0 <= r < bs and 0 <= c < bs and board[r, c] == color:
                count += 1
                r += sign * dr
                c += sign * dc
        if count >= 5:
            return True
    return False


def _immediate_wins(board, color, bs, limit=None):
    wins = []
    for move in np.flatnonzero(board.reshape(-1) == 0):
        if _wins_at(board, int(move), color, bs):
            wins.append(int(move))
            if limit is not None and len(wins) >= limit:
                break
    return wins


def _validate_records(path, data, bs, max_policy):
    if np.any((data["stm"] != 0) & (data["stm"] != 1)):
        raise ValueError(f"{path}: side_to_move must be 0 or 1")
    if np.any((data["feat"] != 0) & (data["feat"] != 1)):
        raise ValueError(f"{path}: feature planes must contain only 0 or 1")
    if np.any(data["pcount"] == 0) or np.any(data["pcount"] > max_policy):
        raise ValueError(f"{path}: policy_count must be between 1 and {max_policy}")

    features = data["feat"].reshape(-1, 2, bs, bs)
    if np.any((features[:, 0] != 0) & (features[:, 1] != 0)):
        raise ValueError(f"{path}: feature planes contain overlapping stones")
    stone_counts = features.sum(axis=(1, 2, 3))
    if np.any(data["ply"] != stone_counts):
        raise ValueError(f"{path}: ply does not match the number of stones")
    expected_stm = (data["ply"] % 2 == 0).astype(np.uint8)
    if np.any(data["stm"] != expected_stm):
        raise ValueError(f"{path}: side_to_move does not match ply parity")

    if np.any(~np.isfinite(data["raw_search"])) or np.any(~np.isfinite(data["played_score"])):
        raise ValueError(f"{path}: raw search scores must be finite")
    for name in ("search", "td"):
        if np.any(np.abs(data[name].astype(np.int32)) > SCORE_SCALE):
            raise ValueError(f"{path}: {name} target is outside [-1, 1]")
    if np.any(~np.isin(data["final"], (-SCORE_SCALE, 0, SCORE_SCALE))):
        raise ValueError(f"{path}: final target must be -1, 0, or 1")

    if np.any(data["depth"] == 0):
        raise ValueError(f"{path}: completed search depth must be at least 1")
    flags = data["flags"]
    if np.any((flags & np.uint16(~KNOWN_FLAGS & 0xFFFF)) != 0):
        raise ValueError(f"{path}: flags contain unknown bits")
    if np.any((flags & SEARCH_VALID) == 0):
        raise ValueError(f"{path}: SEARCH_VALID flag is required")
    if np.any(~np.isin(data["end_reason"], (1, 2))):
        raise ValueError(f"{path}: end_reason must be 1 or 2")
    if np.any(~np.isin(data["tactical"], (-1, 0, 1))):
        raise ValueError(f"{path}: tactical value must be -1, 0, or 1")

    area = bs * bs
    occupied = features[:, 0] | features[:, 1]
    valid_policy = np.arange(max_policy)[None, :] < data["pcount"][:, None]
    policy_moves = data["policy"]["move"]
    probabilities = data["policy"]["prob"]
    if np.any(policy_moves[valid_policy] >= area):
        raise ValueError(f"{path}: policy move index is outside the board")
    if np.any(probabilities[valid_policy] == 0):
        raise ValueError(f"{path}: policy probabilities must be positive")
    probability_sums = np.where(valid_policy, probabilities, 0).astype(np.uint32).sum(
        axis=1
    ) / 65535.0
    if np.any(np.abs(probability_sums - 1.0) > POLICY_SUM_TOLERANCE):
        raise ValueError(f"{path}: policy probabilities do not sum to 1 within rounding tolerance")

    for index in range(len(data)):
        count = int(data["pcount"][index])
        moves = policy_moves[index, :count]
        if len(np.unique(moves)) != count:
            raise ValueError(f"{path}: policy contains duplicate moves")
        if occupied[index].reshape(-1)[moves].any():
            raise ValueError(f"{path}: policy move is not an empty square")
        played_move = int(data["played_move"][index])
        if played_move >= area:
            raise ValueError(f"{path}: played move index is outside the board")
        if occupied[index].reshape(-1)[played_move]:
            raise ValueError(f"{path}: played move is not an empty square")

        own, opponent = features[index]
        if data["stm"][index] == 1:
            black, white = own, opponent
        else:
            black, white = opponent, own
        board = np.where(black, 1, np.where(white, 2, 0)).astype(np.uint8)
        tactical = int(data["tactical"][index])
        if tactical:
            own_color = 1 if data["stm"][index] == 1 else 2
            opponent_color = 3 - own_color
            if tactical == 1 and not _wins_at(
                board, int(data["played_move"][index]), own_color, bs
            ):
                raise ValueError(f"{path}: tactical=1 requires an immediate win on the board")
            if tactical == -1:
                own_wins = _immediate_wins(board, own_color, bs, limit=1)
                opponent_wins = _immediate_wins(board, opponent_color, bs, limit=2)
                if own_wins or len(opponent_wins) < 2:
                    raise ValueError(
                        f"{path}: tactical=-1 requires at least two opponent wins and no own win"
                    )

        key = canonical_position_key(own, opponent, data["stm"][index], bs)
        if fnv1a64(key) != int(data["position_key"][index]):
            raise ValueError(f"{path}: position_key does not match the canonical board key")

    black_result = data["final"].astype(np.int32) * np.where(data["stm"] == 1, 1, -1)
    if np.any((data["end_reason"] == 1) & (black_result == 0)) or np.any(
        (data["end_reason"] == 2) & (black_result != 0)
    ):
        raise ValueError(f"{path}: end_reason does not match the final game result")
    game_metadata = {}
    for index, game_id in enumerate(data["game_id"]):
        metadata = (
            int(data["opening_id"][index]),
            int(data["end_reason"][index]),
            int(black_result[index]),
        )
        prior = game_metadata.setdefault(int(game_id), metadata)
        if prior != metadata:
            raise ValueError(f"{path}: game metadata is inconsistent for game_id {int(game_id)}")


def load_shard(path):
    """
    Load a single shard file from the versioned NNUE dataset.
    @param path ; str ; path to a <name>-NNNNN.bin file
    @return dict ; arrays (X, stm, ply, search, final, td, policy_move,
                   policy_prob, policy_count) plus metadata (bs, scale)
    """
    with open(path, "rb") as f:
        header_data = np.fromfile(f, dtype=HEADER_DTYPE, count=1)
        if len(header_data) != 1:
            raise ValueError(f"{path}: truncated dataset header")
        header = header_data[0]
        if int(header["magic"]) != MAGIC:
            raise ValueError(f"{path}: invalid magic (not a GNN1 dataset)")
        version = int(header["version"])
        if version != VERSION:
            raise ValueError(f"{path}: unsupported dataset version {version}")

        bs = int(header["board_size"])
        flen = int(header["feature_len"])
        scale = int(header["score_scale"])
        max_policy = int(header["max_policy"])
        n = int(header["sample_count"])
        if not MIN_BOARD_SIZE <= bs <= MAX_BOARD_SIZE:
            raise ValueError(f"{path}: invalid board size {bs}")
        if int(header["rule"]) != 0:
            raise ValueError(f"{path}: unsupported rule {int(header['rule'])}")
        if flen != 2 * bs * bs:
            raise ValueError(f"{path}: feature length {flen} does not match board size {bs}")
        if scale != SCORE_SCALE:
            raise ValueError(f"{path}: unsupported score scale {scale}")
        if max_policy != MAX_POLICY:
            raise ValueError(f"{path}: unsupported maximum policy count {max_policy}")
        if int(header["reserved0"]) != 0 or np.any(header["reserved"] != 0):
            raise ValueError(f"{path}: reserved header fields must be zero")

        rec = _record_dtype(flen, max_policy)
        expected_size = HEADER_DTYPE.itemsize + n * rec.itemsize
        actual_size = os.fstat(f.fileno()).st_size
        if actual_size != expected_size:
            raise ValueError(
                f"{path}: invalid shard size; expected {expected_size} bytes, found {actual_size}"
            )
        data = np.fromfile(f, dtype=rec, count=n)
        if len(data) != n:
            raise ValueError(f"{path}: truncated sample data")

    _validate_records(path, data, bs, max_policy)

    X = data["feat"].reshape(n, 2, bs, bs)
    return {
        "X": X,
        "stm": data["stm"],
        "ply": data["ply"],
        "search": data["search"].astype(np.float32) / scale,
        "final": data["final"].astype(np.float32) / scale,
        "td": data["td"].astype(np.float32) / scale,
        "policy_move": data["policy"]["move"],
        "policy_prob": data["policy"]["prob"].astype(np.float32) / 65535.0,
        "policy_count": data["pcount"],
        "game_id": data["game_id"],
        "opening_id": data["opening_id"],
        "position_key": data["position_key"],
        "raw_search": data["raw_search"],
        "played_score": data["played_score"],
        "nodes": data["nodes"],
        "depth": data["depth"],
        "played_move": data["played_move"],
        "flags": data["flags"],
        "end_reason": data["end_reason"],
        "tactical": data["tactical"],
        "bs": bs,
        "scale": scale,
    }


def load_dataset(prefix):
    """
    Load and concatenate all shards for a dataset prefix.
    @param prefix ; str ; prefix without shard suffix (e.g. "nnue_dataset")
                   or an explicit shard file path
    @return dict ; same structure as load_shard, concatenated across shards
    """
    if os.path.isfile(prefix):
        paths = [prefix]
    else:
        paths = sorted(glob.glob(prefix + "-*.bin"))
    if not paths:
        raise FileNotFoundError(f"no shards found for prefix '{prefix}'")

    shards = [load_shard(p) for p in paths]
    out = {"bs": shards[0]["bs"], "scale": shards[0]["scale"]}
    for path, shard in zip(paths[1:], shards[1:]):
        if shard["bs"] != out["bs"] or shard["scale"] != out["scale"]:
            raise ValueError(f"{path}: shard metadata does not match the first shard")
    keys = [
        "X", "stm", "ply", "search", "final", "td",
        "policy_move", "policy_prob", "policy_count", "game_id", "opening_id",
        "position_key", "raw_search", "played_score", "nodes", "depth", "played_move",
        "flags", "end_reason", "tactical",
    ]
    for k in keys:
        out[k] = np.concatenate([s[k] for s in shards], axis=0)
    black_result = out["final"].astype(np.int32) * np.where(out["stm"] == 1, 1, -1)
    game_metadata = {}
    for index, game_id in enumerate(out["game_id"]):
        metadata = (
            int(out["opening_id"][index]),
            int(out["end_reason"][index]),
            int(black_result[index]),
        )
        prior = game_metadata.setdefault(int(game_id), metadata)
        if prior != metadata:
            raise ValueError(f"{prefix}: game metadata is inconsistent for game_id {int(game_id)}")
    return out


def _sym_plane(a, sym):
    """
    Apply one of the 8 D4 symmetries to a 2D array.
    @param a ; np.ndarray ; square 2D array
    @param sym ; int ; symmetry index 0..7
    @return np.ndarray ; transformed 2D array
    """
    if sym == 0:
        return a
    if sym == 1:
        return np.rot90(a, 1)
    if sym == 2:
        return np.rot90(a, 2)
    if sym == 3:
        return np.rot90(a, 3)
    if sym == 4:
        return np.fliplr(a)
    if sym == 5:
        return np.flipud(a)
    if sym == 6:
        return a.T
    return np.fliplr(np.rot90(a, 1))


def sym_permutation(bs, sym):
    """
    Return the positional permutation (old -> new) for a D4 symmetry,
    consistent with the plane transform in _sym_plane.
    @param bs ; int ; board size
    @param sym ; int ; symmetry index 0..7
    @return np.ndarray ; int64 array of length bs*bs with old_to_new mapping
    """
    idx = np.arange(bs * bs).reshape(bs, bs)
    moved = _sym_plane(idx, sym).reshape(-1)
    perm = np.empty(bs * bs, dtype=np.int64)
    perm[moved] = np.arange(bs * bs)
    return perm


def augment(X, policy_move, policy_count, bs, sym):
    """
    Apply a D4 symmetry to both the feature planes and the policy moves.
    The tuple API remains (transformed features, transformed policy moves); callers
    that use played_move for training must transform it with sym_permutation as well.
    @param X ; np.ndarray ; (2, bs, bs) int8 planes for one sample
    @param policy_move ; np.ndarray ; policy move indices (max_policy,)
    @param policy_count ; int ; number of valid policy entries
    @param bs ; int ; board size
    @param sym ; int ; symmetry index 0..7
    @return tuple ; (X_t (2,bs,bs), policy_move_t)
    """
    x_t = np.stack([_sym_plane(X[0], sym), _sym_plane(X[1], sym)])
    perm = sym_permutation(bs, sym)
    pm = policy_move.copy()
    k = int(policy_count)
    if k > 0:
        pm[:k] = perm[policy_move[:k].astype(np.int64)]
    return x_t, pm


def policy_to_dense(policy_move, policy_prob, policy_count, bs):
    """
    Convert a sparse policy target into a dense bs*bs probability target.
    @param policy_move ; np.ndarray ; move indices (max_policy,)
    @param policy_prob ; np.ndarray ; probabilities (max_policy,)
    @param policy_count ; int ; number of valid entries
    @param bs ; int ; board size
    @return np.ndarray ; (bs*bs,) float32 policy target
    """
    dense = np.zeros(bs * bs, dtype=np.float32)
    k = int(policy_count)
    if k > 0:
        dense[policy_move[:k].astype(np.int64)] = policy_prob[:k]
    return dense


def render(sample_x):
    """
    Render an STM-encoded sample tensor (2, bs, bs) as ASCII.
    Plane 0 = side to move (X), Plane 1 = opponent (O).
    @param sample_x ; np.ndarray ; (2, bs, bs) int8
    @return str ; multi-line board rendering
    """
    own, opp = sample_x[0], sample_x[1]
    bs = own.shape[0]
    lines = []
    for r in range(bs):
        row = []
        for c in range(bs):
            if own[r, c]:
                row.append("X")
            elif opp[r, c]:
                row.append("O")
            else:
                row.append(".")
        lines.append(" ".join(row))
    return "\n".join(lines)


def show_examples(prefix, count=3):
    """
    Print a few examples from the dataset (board, values, policy).
    @param prefix ; str ; dataset prefix or shard path
    @param count ; int ; number of examples to display
    @return void
    """
    ds = load_dataset(prefix)
    bs = ds["bs"]
    n = len(ds["X"])
    print(f"Dataset: {n} positions, board size {bs}")
    print(f"search min/max/mean: {ds['search'].min():.2f} / "
          f"{ds['search'].max():.2f} / {ds['search'].mean():.2f}")
    print(f"final distribution: +1={np.mean(ds['final'] > 0):.2%} "
          f"0={np.mean(ds['final'] == 0):.2%} -1={np.mean(ds['final'] < 0):.2%}\n")

    idx = np.random.choice(n, size=min(count, n), replace=False)
    for i in idx:
        stm = "black" if ds["stm"][i] == 1 else "white"
        print(f"--- Sample {i} | ply {int(ds['ply'][i])} | to move: {stm} "
              f"| stones: {int(ds['X'][i].sum())} ---")
        print(f"search={ds['search'][i]:+.2f} final={ds['final'][i]:+.2f} "
              f"td={ds['td'][i]:+.2f}")
        k = int(ds["policy_count"][i])
        moves = ds["policy_move"][i][:k]
        probs = ds["policy_prob"][i][:k]
        pol = ", ".join(f"{int(m)}:{p:.2f}" for m, p in zip(moves, probs))
        print(f"policy[{k}]: {pol}")
        print(render(ds["X"][i]))
        print()


if __name__ == "__main__":
    show_examples("nnue_dataset", count=3)