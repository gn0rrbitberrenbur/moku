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
    ])


def load_shard(path):
    """
    Load a single shard file from the versioned NNUE dataset.
    @param path ; str ; path to a <name>-NNNNN.bin file
    @return dict ; arrays (X, stm, ply, search, final, td, policy_move,
                   policy_prob, policy_count) plus metadata (bs, scale)
    """
    with open(path, "rb") as f:
        header = np.fromfile(f, dtype=HEADER_DTYPE, count=1)[0]
        if int(header["magic"]) != MAGIC:
            raise ValueError(f"{path}: invalid magic (not a GNN1 dataset)")
        bs = int(header["board_size"])
        flen = int(header["feature_len"])
        scale = int(header["score_scale"])
        max_policy = int(header["max_policy"])
        n = int(header["sample_count"])

        rec = _record_dtype(flen, max_policy)
        data = np.fromfile(f, dtype=rec, count=n)

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
        "bs": bs,
        "scale": scale,
    }


def load_dataset(prefix):
    """
    Load and concatenate all shards for a dataset prefix.
    @param prefix ; str ; prefix without shard suffix (e.g. "nnue_dataset")
                   or an explicit file/glob path
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
    keys = ["X", "stm", "ply", "search", "final", "td",
            "policy_move", "policy_prob", "policy_count"]
    for k in keys:
        out[k] = np.concatenate([s[k] for s in shards], axis=0)
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