"""Charts and LaTeX tables for the report, generated from the CSV results.

Usage (from the repository root):
    python3 -m venv .venv && .venv/bin/pip install matplotlib
    .venv/bin/python scripts/plots.py [--results results] [--out report]

For every scenario (n, glutton, think/eat times) the newest valid run of each
solution is used. A run is valid when it lasted as long as requested; runs that
took much longer (e.g. the machine went to sleep) are skipped and listed.
Charts go to <out>/figures/*.pdf, tables to <out>/generated/*.tex.
"""

import argparse
import csv
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

SOLUTIONS = ["hierarchy", "waiter", "fair"]
LABELS = {"hierarchy": "hierarchia", "waiter": "kelner", "fair": "monitor (fair)"}
# Categorical slots 1-3 of the reference palette, validated for all pairs.
COLORS = {"hierarchy": "#2a78d6", "waiter": "#eb6834", "fair": "#1baf7a"}
TEXT = "#52514e"
GRID = "#e4e3df"

SCENARIOS = {
    "n5": {"n": 5, "glutton": 0, "title": "N = 5"},
    "glutton": {"n": 5, "glutton": 1, "title": "N = 5, żarłok"},
    "n50": {"n": 50, "glutton": 0, "title": "N = 50"},
}


def read_csv(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def is_valid(run):
    return abs(float(run["elapsed_s"]) - int(run["time_s"])) < 0.05 * int(run["time_s"])


def select_runs(runs):
    """Newest valid run per (scenario, solution)."""
    chosen, skipped = {}, []
    for run in runs:
        if run["solution"] == "deadlock":
            continue
        if not is_valid(run):
            skipped.append(run)
            continue
        for key, sc in SCENARIOS.items():
            if int(run["n"]) == sc["n"] and int(run["glutton"]) == sc["glutton"]:
                chosen[(key, run["solution"])] = run  # rows are chronological
    return chosen, skipped


def fmt(x, digits=2):
    return f"{float(x):.{digits}f}".replace(".", ",")


def style(ax):
    ax.spines[["top", "right"]].set_visible(False)
    ax.spines[["left", "bottom"]].set_color(GRID)
    ax.tick_params(colors=TEXT, labelsize=9)
    ax.yaxis.label.set_color(TEXT)
    ax.xaxis.label.set_color(TEXT)
    ax.grid(axis="y", color=GRID, linewidth=0.8)
    ax.set_axisbelow(True)


def save(fig, out, name):
    fig.tight_layout()
    fig.savefig(out / f"{name}.pdf")
    plt.close(fig)


def meals_bars(out, phil, scenario, name):
    """Grouped bars: meals per philosopher (N = 5)."""
    fig, ax = plt.subplots(figsize=(6.4, 3.2))
    width = 0.26
    for k, sol in enumerate(SOLUTIONS):
        rows = phil[(scenario, sol)]
        xs = [int(r["id"]) + (k - 1) * (width + 0.02) for r in rows]
        ax.bar(xs, [int(r["meals"]) for r in rows], width, color=COLORS[sol], label=LABELS[sol])
    ax.set_xticks(range(len(phil[(scenario, SOLUTIONS[0])])))
    ax.set_xlabel("filozof")
    ax.set_ylabel("liczba posiłków")
    ax.legend(frameon=False, fontsize=9, ncol=3, loc="upper center", bbox_to_anchor=(0.5, 1.15))
    style(ax)
    save(fig, out, name)


def per_philosopher_lines(out, phil, scenario, column, ylabel, name):
    """One line per solution across all philosophers (N = 50)."""
    fig, ax = plt.subplots(figsize=(6.4, 3.2))
    for sol in SOLUTIONS:
        rows = phil[(scenario, sol)]
        ax.plot([int(r["id"]) for r in rows], [float(r[column]) for r in rows],
                color=COLORS[sol], linewidth=2, marker="o", markersize=3, label=LABELS[sol])
    ax.set_xlabel("filozof")
    ax.set_ylabel(ylabel)
    ax.set_ylim(bottom=0)
    ax.legend(frameon=False, fontsize=9, ncol=3, loc="upper center", bbox_to_anchor=(0.5, 1.15))
    style(ax)
    save(fig, out, name)


def wait_cdf(out, waits, scenario, name):
    """Empirical CDF of all wait times."""
    fig, ax = plt.subplots(figsize=(6.4, 3.2))
    for sol in SOLUTIONS:
        w = sorted(waits[(scenario, sol)])
        ax.plot(w, [(i + 1) / len(w) for i in range(len(w))],
                color=COLORS[sol], linewidth=2, label=LABELS[sol])
    ax.set_xlabel("czas oczekiwania [ms]")
    ax.set_ylabel("odsetek oczekiwań ≤ x")
    ax.set_xlim(left=0)
    ax.set_ylim(0, 1.01)
    ax.legend(frameon=False, fontsize=9, loc="lower right")
    style(ax)
    ax.grid(axis="x", color=GRID, linewidth=0.8)
    save(fig, out, name)


def wait_over_time(out, waits_t, duration, scenario, name):
    """Max wait in every 1-second window - shows whether waits grow over time."""
    fig, ax = plt.subplots(figsize=(6.4, 3.0))
    for sol in SOLUTIONS:
        buckets = {}
        for t, w in waits_t[(scenario, sol)]:
            sec = int(t // 1000)
            buckets[sec] = max(buckets.get(sec, 0), w)
        secs = sorted(s for s in buckets if s < duration)  # the last window is partial
        ax.plot(secs, [buckets[s] for s in secs], color=COLORS[sol], linewidth=2, label=LABELS[sol])
    ax.set_xlabel("czas od startu [s]")
    ax.set_ylabel("maks. oczekiwanie w sekundzie [ms]")
    ax.set_ylim(bottom=0)
    ax.legend(frameon=False, fontsize=9, ncol=3, loc="upper center", bbox_to_anchor=(0.5, 1.17))
    style(ax)
    save(fig, out, name)


def throughput_panels(out, chosen, name):
    """Small multiples (separate y scales): throughput for each scenario."""
    fig, axes = plt.subplots(1, len(SCENARIOS), figsize=(7.0, 2.8))
    for ax, (key, sc) in zip(axes, SCENARIOS.items()):
        vals = [float(chosen[(key, s)]["throughput"]) for s in SOLUTIONS]
        bars = ax.bar(range(3), vals, 0.7, color=[COLORS[s] for s in SOLUTIONS])
        for b, v in zip(bars, vals):
            ax.text(b.get_x() + b.get_width() / 2, v, fmt(v, 1), ha="center", va="bottom",
                    fontsize=8, color=TEXT)
        ax.set_title(sc["title"], fontsize=10, color=TEXT)
        ax.set_xticks(range(3), [LABELS[s].split()[0] for s in SOLUTIONS])
        ax.set_ylim(0, max(vals) * 1.18)
        style(ax)
        ax.tick_params(axis="x", labelsize=7.5)
    axes[0].set_ylabel("posiłki / s")
    save(fig, out, name)


def summary_table(gen, chosen):
    lines = [r"\begin{tabular}{llrrrrrrrr}", r"\toprule",
             r"scenariusz & rozwiązanie & posiłki & posiłki/s & min & max & max/min & Jain"
             r" & śr. ocz. & p99 ocz. \\",
             r" & & & & & & & & [ms] & [ms] \\", r"\midrule"]
    for key, sc in SCENARIOS.items():
        for i, sol in enumerate(SOLUTIONS):
            r = chosen[(key, sol)]
            head = sc["title"] if i == 0 else ""
            lines.append(
                f"{head} & {LABELS[sol]} & {r['total_meals']} & {fmt(r['throughput'], 1)}"
                f" & {r['min_meals']} & {r['max_meals']} & {fmt(r['max_min_ratio'])}"
                f" & {fmt(r['jain'], 4)} & {fmt(r['avg_wait_ms'], 1)} & {fmt(r['p99_wait_ms'], 1)} \\\\")
        lines.append(r"\midrule" if key != list(SCENARIOS)[-1] else r"\bottomrule")
    lines.append(r"\end{tabular}")
    (gen / "summary.tex").write_text("\n".join(lines) + "\n")


def wait_table(gen, chosen):
    lines = [r"\begin{tabular}{llrrrrr}", r"\toprule",
             r"scenariusz & rozwiązanie & średnia & p50 & p95 & p99 & maks. \\", r"\midrule"]
    for key, sc in SCENARIOS.items():
        for i, sol in enumerate(SOLUTIONS):
            r = chosen[(key, sol)]
            head = sc["title"] if i == 0 else ""
            lines.append(
                f"{head} & {LABELS[sol]} & {fmt(r['avg_wait_ms'], 1)} & {fmt(r['p50_wait_ms'], 1)}"
                f" & {fmt(r['p95_wait_ms'], 1)} & {fmt(r['p99_wait_ms'], 1)}"
                f" & {fmt(r['max_wait_ms'], 1)} \\\\")
        lines.append(r"\midrule" if key != list(SCENARIOS)[-1] else r"\bottomrule")
    lines.append(r"\end{tabular}")
    (gen / "waits.tex").write_text("\n".join(lines) + "\n")


def philosophers_table(gen, phil, scenario, name):
    lines = [r"\begin{tabular}{l" + "r" * 15 + "}", r"\toprule",
             r" & \multicolumn{5}{c}{posiłki} & \multicolumn{5}{c}{śr. oczekiwanie [ms]}"
             r" & \multicolumn{5}{c}{maks. oczekiwanie [ms]} \\",
             r"\cmidrule(lr){2-6}\cmidrule(lr){7-11}\cmidrule(lr){12-16}",
             "filozof & " + " & ".join(["0", "1", "2", "3", "4"] * 3) + r" \\", r"\midrule"]
    for sol in SOLUTIONS:
        rows = phil[(scenario, sol)]
        cells = [r["meals"] for r in rows]
        cells += [fmt(r["avg_wait_ms"], 1) for r in rows]
        cells += [fmt(r["max_wait_ms"], 0) for r in rows]
        lines.append(f"{LABELS[sol]} & " + " & ".join(cells) + r" \\")
    lines += [r"\bottomrule", r"\end{tabular}"]
    (gen / f"{name}.tex").write_text("\n".join(lines) + "\n")


def deadlock_table(gen, runs):
    lines = [r"\begin{tabular}{rrlrr}", r"\toprule",
             r"nr & N & pauza [ms] & posiłki przed zakleszczeniem & wykrycie po [s] \\", r"\midrule"]
    k = 0
    for r in runs:
        if r["solution"] != "deadlock" or int(r["time_s"]) < 60:
            continue
        k += 1
        detected = fmt(r["deadlock_at_s"]) if r["deadlock"] == "1" else "brak"
        lines.append(f"{k} & {r['n']} & {r['pause_min_ms']}-{r['pause_max_ms']}"
                     f" & {r['total_meals']} & {detected} \\\\")
    lines += [r"\bottomrule", r"\end{tabular}"]
    (gen / "deadlock.tex").write_text("\n".join(lines) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--results", default="results")
    ap.add_argument("--out", default="report")
    args = ap.parse_args()

    res = Path(args.results)
    out = Path(args.out)
    figs = out / "figures"
    gen = out / "generated"
    figs.mkdir(parents=True, exist_ok=True)
    gen.mkdir(parents=True, exist_ok=True)

    runs = read_csv(res / "runs.csv")
    chosen, skipped = select_runs(runs)
    for r in skipped:
        print(f"skipped invalid run {r['run_id']}: elapsed {r['elapsed_s']} s, expected {r['time_s']} s")

    phil, waits, waits_t = {}, {}, {}
    for key, run in chosen.items():
        print(f"{key[0]:8} {key[1]:10} <- {run['run_id']}")
        phil[key] = read_csv(res / f"{run['run_id']}_philosophers.csv")
        rows = read_csv(res / f"{run['run_id']}_waits.csv")
        waits[key] = [float(r["wait_ms"]) for r in rows]
        waits_t[key] = [(float(r["t_hungry_ms"]), float(r["wait_ms"])) for r in rows]

    plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 9})
    meals_bars(figs, phil, "n5", "meals_n5")
    meals_bars(figs, phil, "glutton", "meals_glutton")
    per_philosopher_lines(figs, phil, "n50", "meals", "liczba posiłków", "meals_n50")
    per_philosopher_lines(figs, phil, "n50", "avg_wait_ms", "śr. oczekiwanie [ms]", "avg_wait_n50")
    wait_cdf(figs, waits, "n5", "wait_cdf_n5")
    wait_cdf(figs, waits, "n50", "wait_cdf_n50")
    wait_over_time(figs, waits_t, int(chosen[("n50", "fair")]["time_s"]), "n50", "wait_over_time_n50")
    throughput_panels(figs, chosen, "throughput")

    summary_table(gen, chosen)
    wait_table(gen, chosen)
    philosophers_table(gen, phil, "n5", "philosophers_n5")
    philosophers_table(gen, phil, "glutton", "philosophers_glutton")
    deadlock_table(gen, runs)
    print(f"figures -> {figs}, tables -> {gen}")


if __name__ == "__main__":
    main()
