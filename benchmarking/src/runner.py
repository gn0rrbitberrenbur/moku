from __future__ import annotations

import json
import os
import platform
import queue
import re
import subprocess
import threading
import time
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import BinaryIO, Callable

from .config import Config, EngineConfig
from .openings import Board, Move, SIZE, load_openings, sha256
from .statistics import summarize

MOVE_PATTERN = re.compile(r"(-?\d+),(-?\d+)")


def reject_nonfinite(value: str) -> object:
    raise ValueError(f"nonfinite JSON value: {value}")


class EngineFailure(Exception):
    def __init__(self, reason: str, detail: str) -> None:
        super().__init__(detail)
        self.reason = reason


class RunCancelled(Exception):
    """Raised after a cooperative stop request; engine cleanup still runs."""


def check_cancelled(stop_event: threading.Event | None) -> None:
    if stop_event is not None and stop_event.is_set():
        raise RunCancelled("Benchmark stop requested")


class EventLog:
    """Stream one JSON document, flushing every event and finalizing after pipe drainage."""

    def __init__(self, path: Path, metadata: dict[str, object], progress_path: Path | None = None) -> None:
        self.stream = path.open("x", encoding="utf-8", newline="\n")
        self.progress_path = progress_path
        self.lock = threading.Lock()
        self.started_ns = time.monotonic_ns()
        self.sequence = 0
        prefix = json.dumps(metadata, allow_nan=False, ensure_ascii=True)
        self.stream.write(prefix[:-1] + ', "events": [\n')
        self.stream.flush()

    def event(self, engine: str, channel: str, ply: int | None, **data: object) -> None:
        with self.lock:
            event: dict[str, object] = {
                "sequence": self.sequence, "engine": engine, "channel": channel,
                "ply": ply, "received_ns": time.monotonic_ns() - self.started_ns,
                **data,
            }
            if self.sequence:
                self.stream.write(",\n")
            self.stream.write(json.dumps(event, allow_nan=False, ensure_ascii=True))
            self.stream.flush()

            metrics = data.get("metrics")
            if self.progress_path is not None and isinstance(metrics, dict):
                with self.progress_path.open("a", encoding="utf-8", newline="\n") as stream:
                    stream.write(
                        json.dumps(
                            {"engine": engine, **metrics},
                            allow_nan=False,
                            ensure_ascii=True,
                        )
                        + "\n"
                    )
                    stream.flush()

            self.sequence += 1

    def finish(self, result: dict[str, object]) -> None:
        with self.lock:
            self.stream.write(
                '\n], "result": ' + json.dumps(result, allow_nan=False, ensure_ascii=True) + "}\n"
            )
            self.stream.close()


class Engine:
    def __init__(
        self, config: EngineConfig, log: EventLog, stop_event: threading.Event | None = None,
    ) -> None:
        check_cancelled(stop_event)
        self.config = config
        self.log = log
        self.stop_event = stop_event
        self.ply: int | None = None
        self.responses: queue.Queue[str | EngineFailure] = queue.Queue()
        self.threads: list[threading.Thread] = []
        self.reader_failures: list[OSError] = []
        try:
            self.process = subprocess.Popen(
                config.command, cwd=config.cwd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=subprocess.PIPE, bufsize=0,
            )
        except OSError as exc:
            log.event(config.name, "manager", None, event="start_failed", error=str(exc))
            raise EngineFailure("crash", f"{config.name}: cannot start: {exc}") from exc
        log.event(config.name, "manager", None, event="process_started", pid=self.process.pid)
        assert self.process.stdout is not None and self.process.stderr is not None
        for channel, pipe in (("stdout", self.process.stdout), ("stderr", self.process.stderr)):
            thread = threading.Thread(
                target=self._read, args=(channel, pipe),
                name=f"{config.name}-{channel}", daemon=True,
            )
            thread.start()
            self.threads.append(thread)

    def _read(self, channel: str, pipe: BinaryIO) -> None:
        try:
            for raw in iter(pipe.readline, b""):
                text = raw.decode("utf-8", errors="replace")
                data: dict[str, object] = {"text": text}
                if text.startswith("[METRICS] "):
                    try:
                        metrics = json.loads(
                            text[len("[METRICS] "):],
                            parse_constant=reject_nonfinite,
                        )
                    except (ValueError, RecursionError) as exc:
                        data["metrics_parse_error"] = str(exc)
                    else:
                        data = {"metrics": metrics}
                self.log.event(self.config.name, channel, self.ply, **data)
                if channel == "stdout":
                    self.responses.put(text.strip())
        except OSError as exc:
            self.reader_failures.append(exc)
            self.log.event(self.config.name, channel, self.ply, event="read_error", error=str(exc))
            self.responses.put(EngineFailure("io_error", f"{self.config.name}: {exc}"))
        finally:
            self.log.event(self.config.name, channel, self.ply, event="eof")
            if channel == "stdout":
                self.responses.put(EngineFailure("crash", f"{self.config.name}: stdout closed"))
            pipe.close()

    def send(self, command: str) -> None:
        if command != "END":
            check_cancelled(self.stop_event)
        self.log.event(self.config.name, "stdin", self.ply, text=command + "\r\n")
        try:
            assert self.process.stdin is not None
            self.process.stdin.write((command + "\r\n").encode("utf-8"))
            self.process.stdin.flush()
        except OSError as exc:
            raise EngineFailure("crash", f"{self.config.name}: cannot write: {exc}") from exc

    def receive(self, deadline: float) -> str:
        while True:
            check_cancelled(self.stop_event)
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise EngineFailure("timeout", f"{self.config.name}: response deadline exceeded")
            try:
                response = self.responses.get(
                    timeout=min(remaining, 0.1) if self.stop_event is not None else remaining,
                )
            except queue.Empty:
                continue
            check_cancelled(self.stop_event)
            break
        if isinstance(response, EngineFailure):
            raise response
        if response.startswith(("ERROR", "UNKNOWN")):
            raise EngineFailure("protocol_error", f"{self.config.name}: {response}")
        return response

    def expect(self, command: str, expected: str, timeout_ms: int) -> None:
        deadline = time.monotonic() + timeout_ms / 1000
        self.send(command)
        while True:
            response = self.receive(deadline)
            if response == expected:
                return
            if not response.startswith(("MESSAGE ", "DEBUG ", "INFO ")):
                raise EngineFailure(
                    "protocol_error", f"{self.config.name}: expected {expected!r}, got {response!r}",
                )

    def initialize(self, config: Config) -> str:
        timeout = config.startup_timeout_ms
        self.expect("START 15", "OK", timeout)
        deadline = time.monotonic() + timeout / 1000
        self.send("ABOUT")
        about = self.receive(deadline)
        if "name=" not in about or "version=" not in about:
            raise EngineFailure("protocol_error", f"{self.config.name}: invalid ABOUT: {about}")
        self.send(f"INFO timeout_turn {config.move_time_ms}")
        self.send(f"INFO depth {config.depth}")
        self.send("INFO rule 0")
        self.send("INFO game_type 2")
        deadline = time.monotonic() + timeout / 1000
        self.send(f"DEBUG_OUTPUT {config.metrics_interval_ms}")
        response = self.receive(deadline)
        if response == "DEBUG_OUTPUT False":
            self.expect(f"DEBUG_OUTPUT {config.metrics_interval_ms}", "DEBUG_OUTPUT True", timeout)
        elif response != "DEBUG_OUTPUT True":
            raise EngineFailure("protocol_error", f"{self.config.name}: invalid DEBUG_OUTPUT reply")
        return about

    def move(self, board: Board, first: bool, config: Config) -> Move:
        self.ply = len(board.moves) + 1
        deadline = time.monotonic() + (
            config.move_time_ms + config.timeout_tolerance_ms
        ) / 1000
        if first:
            self.send("BOARD")
            for move in board.moves:
                field = 1 if board.cells[move] == board.turn else 2
                self.send(f"{move[0]},{move[1]},{field}")
            self.send("DONE")
        else:
            x, y = board.moves[-1]
            self.send(f"TURN {x},{y}")
        while True:
            response = self.receive(deadline)
            match = MOVE_PATTERN.fullmatch(response)
            if match:
                return int(match[1]), int(match[2])
            if not response.startswith(("MESSAGE ", "DEBUG ", "INFO ", "[METRICS] ")):
                raise EngineFailure(
                    "protocol_error", f"{self.config.name}: unexpected move response {response!r}",
                )

    def check_display(self, board: Board, timeout_ms: int) -> None:
        deadline = time.monotonic() + timeout_ms / 1000
        self.send("DISPLAY")
        rows: dict[int, list[str]] = {}
        while True:
            response = self.receive(deadline)
            if response.startswith("DEBUG White stones:"):
                break
            tokens = response.split()
            if len(tokens) == SIZE + 1 and tokens[0].isdigit():
                rows[int(tokens[0]) - 1] = tokens[1:]
        expected = {
            y: [{1: "X", 2: "O"}.get(board.cells.get((x, y), 0), ".") for x in range(SIZE)]
            for y in range(SIZE)
        }
        if rows != expected:
            raise EngineFailure(
                "incompatible_board", f"{self.config.name}: DISPLAY disagrees with imported position",
            )

    def close(self) -> None:
        try:
            if self.process.poll() is None:
                try:
                    self.send("END")
                except EngineFailure as exc:
                    self.log.event(
                        self.config.name, "manager", self.ply,
                        event="shutdown_write_failed", error=str(exc),
                    )
                try:
                    self.process.wait(timeout=0.3)
                except subprocess.TimeoutExpired:
                    self.log.event(self.config.name, "manager", self.ply, event="terminate")
                    self.process.terminate()
                    try:
                        self.process.wait(timeout=1)
                    except subprocess.TimeoutExpired:
                        self.log.event(self.config.name, "manager", self.ply, event="kill")
                        self.process.kill()
                        self.process.wait(timeout=3)
        finally:
            if self.process.stdin is not None:
                try:
                    self.process.stdin.close()
                except OSError as exc:
                    self.reader_failures.append(exc)
            for thread in self.threads:
                thread.join(timeout=3)
            if any(thread.is_alive() for thread in self.threads):
                raise RuntimeError(f"{self.config.name}: reader thread did not stop")
        self.log.event(
            self.config.name, "manager", self.ply,
            event="process_exited", returncode=self.process.returncode,
        )
        if self.reader_failures:
            raise OSError(f"{self.config.name}: pipe reader failed: {self.reader_failures}")


def close_engines(engines: list[Engine]) -> None:
    failures: list[Exception] = []
    for engine in engines:
        try:
            engine.close()
        except (OSError, RuntimeError, subprocess.SubprocessError) as exc:
            failures.append(exc)
    if failures:
        raise ExceptionGroup("engine shutdown failed", failures)


def preflight(
    config: Config, folder: Path, stop_event: threading.Event | None = None,
) -> list[str]:
    log = EventLog(folder / "preflight.json", {"schema_version": 1, "kind": "preflight"})
    engines: list[Engine] = []
    identities: list[str] = []
    result: dict[str, object] = {"status": "failed"}
    try:
        for engine_config in config.engines:
            engine = Engine(engine_config, log, stop_event)
            engines.append(engine)
            identities.append(engine.initialize(config))
            # BOARD is sent only to the side to move: equal counts for black,
            # one extra opponent stone for white.
            for moves in ([(7, 7), (8, 7)], [(7, 7)]):
                engine.ply = None
                engine.expect("START 15", "OK", config.startup_timeout_ms)
                board = Board()
                for move in moves:
                    board.play(move)
                move = engine.move(board, True, config)
                try:
                    board.play(move)
                except ValueError as exc:
                    raise EngineFailure("illegal_move", f"preflight: {exc}") from exc
                engine.check_display(board, config.startup_timeout_ms)
        result["status"] = "passed"
        result["identities"] = identities
        return identities
    except RunCancelled as exc:
        result.update(status="interrupted", reason="cancelled", error=str(exc))
        raise
    except EngineFailure as exc:
        result.update(reason=exc.reason, error=str(exc))
        raise
    finally:
        try:
            close_engines(engines)
        except (OSError, RuntimeError, subprocess.SubprocessError, ExceptionGroup) as exc:
            result.update(status="failed", shutdown_error=str(exc))
            raise
        finally:
            log.finish(result)


def play_game(
    config: Config, folder: Path, game_id: int, opening_id: str,
    opening: list[Move], candidate_black: bool, progress_path: Path | None = None,
    stop_event: threading.Event | None = None,
) -> dict[str, object]:
    black_index = 1 if candidate_black else 0
    color_engines = {1: black_index, 2: 1 - black_index}
    log = EventLog(folder / f"game-{game_id:03}.json", {
        "schema_version": 1, "game_id": game_id, "opening_id": opening_id,
        "opening_moves": opening, "board_size": SIZE, "rule": "freestyle",
        "black": config.engines[black_index].name,
        "white": config.engines[1 - black_index].name,
    }, progress_path)
    board = Board()
    for move in opening:
        board.play(move)
    engines: list[Engine] = []
    result: dict[str, object] = {"status": "interrupted"}
    active = 0
    try:
        for active, engine_config in enumerate(config.engines):
            engine = Engine(engine_config, log, stop_event)
            engines.append(engine)
            engine.initialize(config)
        first = [True, True]
        while len(board.moves) < SIZE * SIZE:
            color = board.turn
            active = color_engines[color]
            start = time.monotonic_ns()
            move = engines[active].move(board, first[active], config)
            elapsed_ms = (time.monotonic_ns() - start) / 1_000_000
            first[active] = False
            try:
                won = board.play(move)
            except ValueError as exc:
                raise EngineFailure("illegal_move", f"{config.engines[active].name}: {exc}") from exc
            log.event(
                config.engines[active].name, "manager", len(board.moves),
                event="move_accepted", move=move, color=color, elapsed_ms=elapsed_ms,
            )
            if won:
                result = {
                    "status": "completed", "termination": "five_or_more",
                    "winner": config.engines[active].name,
                    "candidate_score": float(active == 1), "technical": False,
                }
                break
        else:
            result = {
                "status": "completed", "termination": "full_board", "winner": None,
                "candidate_score": 0.5, "technical": False,
            }
    except RunCancelled as exc:
        result.update(termination="cancelled", error=str(exc))
        raise
    except EngineFailure as exc:
        result = {
            "status": "completed", "termination": exc.reason, "error": str(exc),
            "winner": config.engines[1 - active].name,
            "candidate_score": float(active == 0), "technical": True,
            "failed_engine": config.engines[active].name,
        }
    finally:
        result["moves"] = board.moves
        result["plies"] = len(board.moves)
        result["candidate_color"] = "black" if candidate_black else "white"
        try:
            close_engines(engines)
        except (OSError, RuntimeError, subprocess.SubprocessError, ExceptionGroup) as exc:
            result.update(status="failed", shutdown_error=str(exc))
            raise
        finally:
            log.finish(result)
    return result


def write_json(path: Path, value: object) -> None:
    temporary = path.with_suffix(".json.tmp")
    with temporary.open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(value, stream, indent=2, allow_nan=False, ensure_ascii=True)
        stream.write("\n")
    temporary.replace(path)


def report(
    pairs: list[tuple[float, float]], games: list[dict[str, object]],
    config: Config, status: object,
) -> dict[str, object]:
    breakdown: dict[str, object] = {}
    for label, selection in (
        ("candidate_black", [g for g in games if g["candidate_color"] == "black"]),
        ("candidate_white", [g for g in games if g["candidate_color"] == "white"]),
        ("technical", [g for g in games if g["technical"]]),
        ("nontechnical", [g for g in games if not g["technical"]]),
    ):
        breakdown[label] = {
            "games": len(selection),
            "wins": sum(g["candidate_score"] == 1 for g in selection),
            "draws": sum(g["candidate_score"] == 0.5 for g in selection),
            "losses": sum(g["candidate_score"] == 0 for g in selection),
        }
    return {
        "status": status, **summarize(pairs, config.sprt),
        "completed_games": len(games), "unpaired_games": len(games) - 2 * len(pairs),
        "technical_games": sum(bool(game["technical"]) for game in games),
        "breakdown": breakdown, "games_detail": games,
    }


def run(
    config: Config, progress: Callable[[str], None] = print,
    progress_path: Path | None = None, *, stop_event: threading.Event | None = None,
) -> Path:
    """Run paired matches; a stop event raises RunCancelled after cleanup and log finalization."""
    openings = load_openings(config.openings)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    folder = config.output_dir / f"{stamp}-{uuid.uuid4().hex[:8]}"
    folder.mkdir(parents=True, exist_ok=False)
    manifest: dict[str, object] = {
        "schema_version": 1, "status": "running", "started_utc": stamp,
        "platform": platform.platform(), "python": platform.python_version(),
        "cpu_count": os.cpu_count(), "config_sha256": sha256(config.source),
        "openings_sha256": sha256(config.openings), "max_pairs": len(openings),
        "source_config": str(config.source),
        "settings": {
            "move_time_ms": config.move_time_ms,
            "timeout_tolerance_ms": config.timeout_tolerance_ms,
            "startup_timeout_ms": config.startup_timeout_ms,
            "metrics_interval_ms": config.metrics_interval_ms, "depth": config.depth,
        },
        "engines": [
            {"name": engine.name, "command": engine.command, "cwd": str(engine.cwd),
             "executable_sha256": sha256(Path(engine.command[0]))}
            for engine in config.engines
        ],
    }
    (folder / "config.toml").write_bytes(config.source.read_bytes())
    (folder / "openings.json").write_bytes(config.openings.read_bytes())
    write_json(folder / "manifest.json", manifest)
    pairs: list[tuple[float, float]] = []
    games: list[dict[str, object]] = []
    try:
        progress(f"Run directory: {folder}")
        check_cancelled(stop_event)
        manifest["identities"] = preflight(config, folder, stop_event)
        for opening_id, moves in openings:
            scores = []
            # Alternate which engine plays black first between opening pairs.
            for candidate_black in ((len(pairs) % 2 == 0), (len(pairs) % 2 != 0)):
                check_cancelled(stop_event)
                game_id = len(games) + 1
                result = play_game(
                    config, folder, game_id, opening_id, moves, candidate_black,
                    progress_path, stop_event,
                )
                games.append({"game_id": game_id, "opening_id": opening_id, **result})
                score = result["candidate_score"]
                assert isinstance(score, float)
                scores.append(score)
                progress(
                    f"Game {game_id:03}/200: {opening_id}, "
                    f"candidate={score:g}, {result['termination']}"
                )
            pairs.append((scores[0], scores[1]))
            statistics = report(pairs, games, config, "running")
            write_json(folder / "summary.json", statistics)
            sprt = statistics["sprt"]
            assert isinstance(sprt, dict)
            if sprt["decision"] in ("accept_h0", "accept_h1"):
                manifest["stop_reason"] = "sprt"
                break
        else:
            manifest["stop_reason"] = "opening_suite_exhausted"
        manifest["status"] = "completed"
    except (Exception, KeyboardInterrupt) as exc:
        manifest.update(
            status="interrupted" if isinstance(exc, (KeyboardInterrupt, RunCancelled)) else "failed",
            error=f"{type(exc).__name__}: {exc}",
        )
        if isinstance(exc, RunCancelled):
            manifest["stop_reason"] = "cancelled"
        raise
    finally:
        manifest["finished_utc"] = datetime.now(timezone.utc).isoformat()
        write_json(folder / "manifest.json", manifest)
        write_json(folder / "summary.json", report(pairs, games, config, manifest["status"]))
    return folder
