from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from .config import load_config
from .openings import generate_openings
from .runner import EngineFailure, run


def main() -> int:
    parser = argparse.ArgumentParser(description="Paired moku extended-protocol matches")
    parser.add_argument("config", type=Path, nargs="?", help="Run configuration in TOML format")
    parser.add_argument(
        "--progress", type=Path, metavar="PATH",
        help="Path to the progress JSONL file",
    )
    parser.add_argument(
        "--generate-openings", type=Path, metavar="PATH",
        help="Create the deterministic 100-opening book; refuses to overwrite a file",
    )
    args = parser.parse_args()
    if args.generate_openings is not None:
        if args.config is not None:
            parser.error("use either a configuration or --generate-openings")
        with args.generate_openings.open("x", encoding="utf-8") as stream:
            json.dump(generate_openings(), stream, indent=2, allow_nan=False)
            stream.write("\n")
        print(f"Generated 100 openings: {args.generate_openings}")
        return 0
    if args.config is None:
        parser.error("a TOML configuration is required")
    try:
        folder = run(load_config(args.config), progress_path=args.progress)
    except KeyboardInterrupt:
        print("Interrupted; completed logs and run status are preserved.", file=sys.stderr)
        return 130
    except (OSError, ValueError, EngineFailure, ExceptionGroup) as exc:
        print(f"Benchmark failed: {exc}", file=sys.stderr)
        return 1
    print(f"Results: {folder}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
