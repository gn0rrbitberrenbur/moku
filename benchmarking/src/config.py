from __future__ import annotations

import math
import shutil
import tomllib
from dataclasses import dataclass
from pathlib import Path
from typing import Mapping


@dataclass(frozen=True)
class EngineConfig:
    name: str
    command: tuple[str, ...]
    cwd: Path


@dataclass(frozen=True)
class SprtConfig:
    enabled: bool
    elo0: float
    elo1: float
    alpha: float
    beta: float
    min_pairs: int


@dataclass(frozen=True)
class TechnicalConfig:
    move_time_ms: int = 1000
    depth: int = 12
    repetitions: int = 5


@dataclass(frozen=True)
class Config:
    source: Path
    output_dir: Path
    openings: Path
    move_time_ms: int
    timeout_tolerance_ms: int
    startup_timeout_ms: int
    metrics_interval_ms: int
    depth: int
    engines: tuple[EngineConfig, EngineConfig]
    sprt: SprtConfig
    max_nodes: int = 0
    technical: TechnicalConfig = TechnicalConfig()


def table(value: object, name: str, keys: set[str]) -> Mapping[str, object]:
    if not isinstance(value, dict) or not all(isinstance(k, str) for k in value):
        raise ValueError(f"{name} must be a table")
    unknown = set(value) - keys
    if unknown:
        raise ValueError(f"{name}: unknown settings {sorted(unknown)}")
    return value


def integer(data: Mapping[str, object], key: str, low: int, high: int) -> int:
    value = data.get(key)
    if type(value) is not int or not low <= value <= high:
        raise ValueError(f"{key} must be an integer in [{low}, {high}]")
    return value


def number(data: Mapping[str, object], key: str) -> float:
    value = data.get(key)
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(f"{key} must be a finite number")
    result = float(value)
    if not math.isfinite(result):
        raise ValueError(f"{key} must be a finite number")
    return result


def text(data: Mapping[str, object], key: str) -> str:
    value = data.get(key)
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{key} must be a nonempty string")
    return value


def load_config(path: Path) -> Config:
    source = path.resolve()
    with source.open("rb") as stream:
        root = table(tomllib.load(stream), "root", {"run", "engines", "sprt", "technical"})
    run = table(
        root.get("run"), "run",
        {"output_dir", "openings", "move_time_ms", "timeout_tolerance_ms",
         "startup_timeout_ms", "metrics_interval_ms", "depth", "max_nodes"},
    )
    base = source.parent
    engines_data = root.get("engines")
    if not isinstance(engines_data, list) or len(engines_data) != 2:
        raise ValueError("exactly two [[engines]] tables are required: reference, candidate")
    engines: list[EngineConfig] = []
    for item in engines_data:
        engine = table(item, "engine", {"name", "command", "cwd"})
        command = engine.get("command")
        if (not isinstance(command, list) or not command
                or not all(isinstance(arg, str) and arg for arg in command)):
            raise ValueError("engine command must be a nonempty array of nonempty strings")
        cwd = (base / text(engine, "cwd")).resolve()
        if not cwd.is_dir():
            raise ValueError(f"engine cwd does not exist: {cwd}")
        executable = Path(command[0])
        local = (base / executable).resolve()
        resolved = str(local) if local.is_file() else None
        if resolved is None and not executable.is_absolute() and executable.name == command[0]:
            resolved = shutil.which(command[0])
        if resolved is None:
            raise ValueError(f"engine executable not found: {command[0]}")
        engines.append(EngineConfig(text(engine, "name"), (resolved, *command[1:]), cwd))
    if engines[0].name == engines[1].name:
        raise ValueError("engine names must be distinct")
    sprt = table(
        root.get("sprt"), "sprt", {"enabled", "elo0", "elo1", "alpha", "beta", "min_pairs"},
    )
    enabled = sprt.get("enabled")
    if type(enabled) is not bool:
        raise ValueError("sprt.enabled must be a boolean")
    elo0, elo1 = number(sprt, "elo0"), number(sprt, "elo1")
    alpha, beta = number(sprt, "alpha"), number(sprt, "beta")
    if not -1000 <= elo0 < elo1 <= 1000:
        raise ValueError("SPRT requires -1000 <= elo0 < elo1 <= 1000")
    if not 0 < alpha < 0.5 or not 0 < beta < 0.5:
        raise ValueError("SPRT alpha and beta must be in (0, 0.5)")
    technical = table(root.get("technical", {}), "technical", {"move_time_ms", "depth", "repetitions"})
    technical_defaults = TechnicalConfig()
    technical_config = TechnicalConfig(
        integer(technical, "move_time_ms", 1, 86_400_000)
        if "move_time_ms" in technical else technical_defaults.move_time_ms,
        integer(technical, "depth", 1, 20) if "depth" in technical else technical_defaults.depth,
        integer(technical, "repetitions", 1, 100)
        if "repetitions" in technical else technical_defaults.repetitions,
    )
    return Config(
        source, (base / text(run, "output_dir")).resolve(),
        (base / text(run, "openings")).resolve(),
        integer(run, "move_time_ms", 1, 86_400_000),
        integer(run, "timeout_tolerance_ms", 0, 60_000),
        integer(run, "startup_timeout_ms", 1, 300_000),
        integer(run, "metrics_interval_ms", 1, 60_000),
        integer(run, "depth", 1, 20),
        (engines[0], engines[1]),
        SprtConfig(enabled, elo0, elo1, alpha, beta, integer(sprt, "min_pairs", 1, 100)),
        integer(run, "max_nodes", 0, 2_147_483_647) if "max_nodes" in run else 0,
        technical_config,
    )
