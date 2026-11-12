from __future__ import annotations

import importlib
import json
import math
from pathlib import Path
from statistics import median
from typing import Any


METRICS = (
    ("depth_completed", "Completed search depth", "depth"),
    ("nps", "Nodes per second", "nodes/s"),
    ("elapsed_ms", "Search elapsed time", "ms"),
    ("nodes_total", "Nodes per search", "nodes"),
)

TECHNICAL_METRICS = (
    ("elapsed_ms", "Search time", "ms"),
    ("nodes_total", "Nodes", "nodes"),
    ("nps", "Nodes per second", "nodes/s"),
    ("tt_hits_total", "TT hits", "hits"),
    ("depth_completed", "Completed depth", "depth"),
    ("tt_capacity_entries", "TT capacity", "entries"),
    ("tt_probes_total", "TT probes", "probes"),
    ("alpha_beta_cutoffs_total", "Alpha-beta cutoffs", "cutoffs"),
    ("root_candidates_count", "Root candidates", "candidates"),
)


def _markdown(value: object) -> str:
    return str(value).replace("\\", "\\\\").replace("|", "\\|").replace("\n", " ")


def _number(value: object) -> float | None:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return None
    result = float(value)
    return result if math.isfinite(result) and result >= 0 else None


def _format(value: object) -> str:
    if value is None:
        return "n/a"
    if isinstance(value, float):
        return f"{value:,.2f}"
    if isinstance(value, list):
        return " to ".join(_format(item) for item in value)
    return _markdown(value)


def check_matplotlib() -> None:
    """Verify report dependencies before starting a potentially long benchmark."""
    try:
        importlib.import_module("matplotlib.figure")
        importlib.import_module("matplotlib.backends.backend_svg")
    except ModuleNotFoundError as exc:
        if exc.name != "matplotlib":
            raise
        raise ValueError(
            "Matplotlib is required for benchmark reports; install it with "
            "python -m pip install -r benchmarking/requirements.txt"
        ) from exc


def _bar_chart(path: Path, title: str, values: list[tuple[str, float]], unit: str) -> None:
    """Write a horizontal bar chart with Matplotlib's non-interactive SVG canvas."""
    from matplotlib.backends.backend_svg import FigureCanvasSVG
    from matplotlib.figure import Figure

    figure = Figure(figsize=(8, max(2.5, 1.4 + 0.5 * len(values))))
    FigureCanvasSVG(figure)
    axes = figure.subplots()
    labels = [label for label, _ in values]
    numbers = [value for _, value in values]
    bars = axes.barh(range(len(values)), numbers, color="#3977a8")
    axes.set_yticks(range(len(values)), labels)
    axes.invert_yaxis()
    axes.set_title(title)
    axes.set_xlabel(unit)
    axes.bar_label(bars, labels=[f"{value:,.2f}" for value in numbers], padding=4)
    axes.set_xlim(0, max(numbers, default=0) * 1.2 or 1)
    figure.savefig(path, format="svg", bbox_inches="tight")


def _technical_report(folder: Path, names: list[str]) -> list[str]:
    path = folder / "technical.json"
    if not path.exists():
        return []
    data: dict[str, Any] = json.loads(path.read_text(encoding="utf-8"))
    settings = data["settings"]
    observations = data["observations"]
    lines = [
        "", "## Technical benchmark", "",
        f"Time: {settings['move_time_ms']} ms; depth: {settings['depth']}; "
        f"node limit: {settings['max_nodes']} (unlimited); "
        f"repetitions per position and engine: {settings['repetitions']}.",
        "Each search uses a fresh process and cold transposition table. The same positions "
        "are used for both engines. Results are medians of matched successful repetitions "
        "with the metric present on both sides; n/a means no comparable data.",
        "Timing and node throughput depend on hardware, search paths and workload; "
        "they do not independently establish playing strength.", "",
    ]
    failures = [item for item in observations if item["status"] != "completed"]
    lines.append(f"Failed observations: {len(failures)} of {len(observations)}.")
    if failures:
        lines.extend(("", "| Position | Repetition | Engine | Error |", "| --- | ---: | --- | --- |"))
        for item in failures:
            lines.append(
                f"| {_markdown(item['position'])} | {item['repetition']} | "
                f"{_markdown(item['engine'])} | {_markdown(item.get('error', item['status']))} |"
            )
    totals: dict[str, tuple[list[float], list[float]]] = {
        key: ([], []) for key, _, _ in TECHNICAL_METRICS
    }
    for position in data["positions"]:
        lines.extend((
            "", f"### {_markdown(position['id'])}: {_markdown(position['description'])}", "",
            "| Metric | Reference median | Candidate median | Change | Matched samples |",
            "| --- | ---: | ---: | ---: | ---: |",
        ))
        for key, title, unit in TECHNICAL_METRICS:
            refs: list[float] = []
            candidates: list[float] = []
            for repetition in range(1, settings["repetitions"] + 1):
                matched = {
                    item["engine"]: item for item in observations
                    if item["position"] == position["id"] and item["repetition"] == repetition
                    and item["status"] == "completed"
                }
                if not all(name in matched for name in names):
                    continue
                first = _number(matched[names[0]]["metrics"].get(key))
                second = _number(matched[names[1]]["metrics"].get(key))
                if first is not None and second is not None:
                    refs.append(first)
                    candidates.append(second)
            totals[key][0].extend(refs)
            totals[key][1].extend(candidates)
            first_median = float(median(refs)) if refs else None
            second_median = float(median(candidates)) if candidates else None
            change = (
                f"{100 * (second_median / first_median - 1):+.2f}%"
                if first_median is not None and first_median > 0 and second_median is not None
                else "n/a"
            )
            lines.append(
                f"| {title} ({unit}) | {_format(first_median)} | {_format(second_median)} "
                f"| {change} | {len(refs)} |"
            )
    for key, title, unit in TECHNICAL_METRICS:
        refs, candidates = totals[key]
        if refs and key in ("elapsed_ms", "nps", "depth_completed"):
            filename = f"technical-{key}.svg"
            _bar_chart(folder / filename, f"Technical: {title}", [
                (names[0], float(median(refs))), (names[1], float(median(candidates))),
            ], unit)
            lines.extend(("", f"![Technical {title}]({filename})"))
    lines.extend((
        "", "Charts show pooled medians across matched position/repetition samples; "
        "the tables provide the per-position breakdown. Raw observations, failures, "
        "positions and transcripts are in `technical.json` and `technical-*.json`.", "",
    ))
    return lines


def create_report(folder: Path) -> Path:
    """Create a Markdown report and SVG charts from a completed benchmark's saved logs."""
    check_matplotlib()
    manifest: dict[str, Any] = json.loads((folder / "manifest.json").read_text(encoding="utf-8"))
    summary: dict[str, Any] = json.loads((folder / "summary.json").read_text(encoding="utf-8"))
    names = [engine["name"] for engine in manifest["engines"]]
    samples: dict[str, dict[str, list[float]]] = {
        name: {key: [] for key, _, _ in METRICS} for name in names
    }
    tt: dict[str, list[float]] = {name: [0.0, 0.0, 0.0] for name in names}
    for game in summary["games_detail"]:
        game_path = folder / f"game-{game['game_id']:03}.json"
        data = json.loads(game_path.read_text(encoding="utf-8"))
        for event in data["events"]:
            name, metrics = event.get("engine"), event.get("metrics")
            if name not in samples or not isinstance(metrics, dict):
                continue
            if metrics.get("event") != "search_finished":
                continue
            for key, _, _ in METRICS:
                value = _number(metrics.get(key))
                if value is not None:
                    samples[name][key].append(value)
            hits = _number(metrics.get("tt_hits_total"))
            probes = _number(metrics.get("tt_probes_total"))
            if hits is not None and probes is not None and probes > 0 and hits <= probes:
                tt[name][0] += hits
                tt[name][1] += probes
                tt[name][2] += 1

    reference, candidate = names
    sprt = summary["sprt"]
    lines = [
        "# Benchmark report",
        "",
        f"- Reference: {_markdown(reference)}",
        f"- Candidate: {_markdown(candidate)}",
        f"- Status: {_format(summary['status'])}; stop reason: {_format(manifest.get('stop_reason'))}",
        f"- Started (UTC): {_format(manifest.get('started_utc'))}",
        f"- Completed games: {summary['completed_games']}; complete pairs: {summary['pairs']}; "
        f"unpaired games: {summary['unpaired_games']}",
        f"- Move time: {manifest['settings']['move_time_ms']} ms; "
        f"timeout tolerance: {manifest['settings']['timeout_tolerance_ms']} ms; "
        f"maximum depth: {manifest['settings']['depth']}; "
        f"requested node limit: {_format(manifest['settings'].get('max_nodes'))} "
        "(0 means unlimited)",
        "",
        "## Playing strength",
        "",
        "The candidate-minus-reference statistics below use complete opening pairs only; "
        "technical games are included.",
        "",
        "| Metric | Value |",
        "| --- | ---: |",
        f"| Wins / draws / losses | {summary['wins']} / {summary['draws']} / {summary['losses']} |",
        f"| Score | {_format(None if summary['score'] is None else 100 * summary['score'])}% |"
        if summary["score"] is not None else "| Score | n/a |",
        f"| Elo difference | {_format(summary['elo_delta'])} |",
        f"| 95% Elo interval | {_format(summary['elo_95_interval'])} |",
        f"| SPRT decision | {_format(sprt['decision'])} |",
        f"| SPRT log-likelihood ratio / bounds | {_format(sprt['llr'])} / "
        f"{_format(sprt['lower_bound'])} to {_format(sprt['upper_bound'])} |",
        "",
        "The Elo interval is an anytime-valid paired Hoeffding interval under the "
        "independent-pair assumption. SPRT error rates are nominal; results depend on "
        "the selected openings and settings, not a universal rating.",
        "",
        "![Candidate results in complete pairs](results.svg)",
        "",
        "## Color and reliability",
        "",
        "These counts include all completed games, including an unpaired game.",
        "",
        "| Candidate games | Games | Wins | Draws | Losses |",
        "| --- | ---: | ---: | ---: | ---: |",
    ]
    for key, label in (
        ("candidate_black", "Black"), ("candidate_white", "White"),
        ("technical", "Technical"), ("nontechnical", "Nontechnical"),
    ):
        item = summary["breakdown"][key]
        lines.append(
            f"| {label} | {item['games']} | {item['wins']} | {item['draws']} | {item['losses']} |"
        )
    lines.extend((
        "",
        "## Search metrics",
        "",
        "Each sample is a `search_finished` snapshot from a completed game, including "
        "technical games. Progress snapshots and preflight are excluded. Values are "
        "per-search medians; sample counts can differ when a field is missing. "
        "Receipt order and ply tags do not establish causal association with a move.",
        "",
        "| Metric | Engine | Median | Samples |",
        "| --- | --- | ---: | ---: |",
    ))
    charts: list[tuple[str, str, list[tuple[str, float]], str]] = []
    for key, title, unit in METRICS:
        values = []
        for name in names:
            observations = samples[name][key]
            value = float(median(observations)) if observations else None
            lines.append(f"| {title} ({unit}) | {_markdown(name)} | {_format(value)} | {len(observations)} |")
            if value is not None:
                values.append((name, value))
        if values:
            charts.append((f"{key}.svg", title, values, unit))
    lines.extend((
        "",
        "TT hit rate is total hits divided by total probes across final snapshots "
        "with valid positive probe counts; it is not the average of individual rates.",
        "",
        "| Engine | TT hit rate | Searches with probes |",
        "| --- | ---: | ---: |",
    ))
    for name in names:
        hits, probes, count = tt[name]
        rate = f"{100 * hits / probes:.2f}%" if probes else "n/a"
        lines.append(f"| {_markdown(name)} | {rate} | {int(count)} |")
    for filename, title, _, _ in charts:
        lines.extend(("", f"![{title}]({filename})"))
    lines.extend((
        "",
        "Missing metrics are shown as n/a, not zero. Search time is engine-reported; "
        "it is distinct from the runner's wall-clock response time.",
        "",
        "See `summary.json`, `manifest.json`, and the game JSON files for detailed "
        "results and raw observations.",
        "",
    ))

    _bar_chart(folder / "results.svg", "Candidate results in complete pairs", [
        ("Wins", summary["wins"]), ("Draws", summary["draws"]), ("Losses", summary["losses"]),
    ], "games")
    for filename, title, values, unit in charts:
        _bar_chart(folder / filename, title, values, unit)
    lines.extend(_technical_report(folder, names))
    path = folder / "report.md"
    path.write_text("\n".join(lines), encoding="utf-8")
    return path
