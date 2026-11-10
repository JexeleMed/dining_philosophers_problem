"""Charts and LaTeX tables for the report, generated from the CSV results.

Usage (from the repository root):
    python3 -m venv .venv && .venv/bin/pip install matplotlib
    .venv/bin/python scripts/plots.py [--results results/v2] [--out report]

Only runs of the measurement series are used (see SERIES). A run also has to last as
long as requested: a sleeping machine stretches it, and such runs are skipped and listed.
Every scenario has several repetitions, which are averaged. Spread in percent always
means (max - min) / mean.
Charts go to <out>/figures/*.pdf, tables to <out>/generated/*.tex.
"""

import argparse
import csv
import math
from pathlib import Path
from statistics import mean

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

SOLUTIONS = ["hierarchy", "waiter", "fair"]
LABELS = {"hierarchy": "hierarchia", "waiter": "kelner", "fair": "monitor"}
# Categorical slots 1-3 of the reference palette, validated for all pairs.
COLORS = {"hierarchy": "#2a78d6", "waiter": "#eb6834", "fair": "#1baf7a"}
MARKERS = {"hierarchy": "o", "waiter": "s", "fair": "^"}  # for black and white print
TEXT = "#52514e"
GRID = "#e4e3df"

SERIES = {"time_s": 60, "think_min_ms": 50, "think_max_ms": 300, "eat_min_ms": 50, "eat_max_ms": 300}
SCENARIOS = {
    "n5": {"n": 5, "glutton": 0, "title": "N = 5"},
    "glutton": {"n": 5, "glutton": 1, "title": "N = 5, żarłok"},
    "n50": {"n": 50, "glutton": 0, "title": "N = 50"},
}


def read_csv(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def in_series(run):
    return all(int(run[k]) == v for k, v in SERIES.items())


def is_valid(run):
    return abs(float(run["elapsed_s"]) - int(run["time_s"])) < 0.01 * int(run["time_s"])


def select_runs(runs):
    """All valid runs of the series, as {(scenario, solution): [run, ...]}."""
    chosen, skipped = {}, []
    for run in runs:
        if run["solution"] == "deadlock" or not in_series(run):
            continue
        if not is_valid(run):
            skipped.append(run)
            continue
        for key, sc in SCENARIOS.items():
            if int(run["n"]) == sc["n"] and int(run["glutton"]) == sc["glutton"]:
                chosen.setdefault((key, run["solution"]), []).append(run)
    return chosen, skipped


def fmt(x, digits=2):
    return f"{float(x):.{digits}f}".replace(".", ",")


def spread(values):
    values = [float(v) for v in values]
    return 100 * (max(values) - min(values)) / mean(values)


def low_high(values, show):
    low, high = show(min(values)), show(max(values))
    return str(low) if low == high else f"{low}-{high}"


def column(rows, name):
    return [float(r[name]) for r in rows]


def percentile(ordered, p):
    return ordered[min(math.ceil(p * len(ordered)) - 1, len(ordered) - 1)]


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


def per_philosopher(phil, scenario, sol, name):
    """Values of one column for every philosopher, one list per repetition."""
    return [[float(r[name]) for r in rows] for rows in phil[(scenario, sol)]]


def meals_bars(out, phil, scenario, name):
    """Grouped bars: mean meals per philosopher, whiskers = min and max of the repetitions."""
    fig, ax = plt.subplots(figsize=(6.4, 3.2))
    width = 0.26
    for k, sol in enumerate(SOLUTIONS):
        reps = per_philosopher(phil, scenario, sol, "meals")
        by_id = list(zip(*reps))
        xs = [i + (k - 1) * (width + 0.02) for i in range(len(by_id))]
        means = [mean(v) for v in by_id]
        low = [m - min(v) for m, v in zip(means, by_id)]
        high = [max(v) - m for m, v in zip(means, by_id)]
        ax.bar(xs, means, width, color=COLORS[sol], label=LABELS[sol], yerr=[low, high],
               error_kw={"ecolor": TEXT, "elinewidth": 1, "capsize": 2})
    ax.set_xticks(range(len(by_id)))
    ax.set_xlabel("filozof")
    ax.set_ylabel("liczba posiłków")
    ax.legend(frameon=False, fontsize=9, ncol=3, loc="upper center", bbox_to_anchor=(0.5, 1.15))
    style(ax)
    save(fig, out, name)


def per_philosopher_lines(out, phil, scenario, name_col, ylabel, name):
    """Mean over repetitions for every philosopher, band = min and max (N = 50)."""
    fig, ax = plt.subplots(figsize=(6.4, 3.2))
    for sol in SOLUTIONS:
        by_id = list(zip(*per_philosopher(phil, scenario, sol, name_col)))
        ids = range(len(by_id))
        ax.fill_between(ids, [min(v) for v in by_id], [max(v) for v in by_id],
                        color=COLORS[sol], alpha=0.18, linewidth=0)
        ax.plot(ids, [mean(v) for v in by_id], color=COLORS[sol], linewidth=2,
                marker=MARKERS[sol], markersize=3, label=LABELS[sol])
    ax.set_xlabel("filozof")
    ax.set_ylabel(ylabel)
    ax.set_ylim(bottom=0)
    ax.legend(frameon=False, fontsize=9, ncol=3, loc="upper center", bbox_to_anchor=(0.5, 1.15))
    style(ax)
    save(fig, out, name)


def wait_cdf(out, waits, scenario, name):
    """Empirical CDF of all waits from all repetitions."""
    fig, ax = plt.subplots(figsize=(6.4, 3.2))
    for sol in SOLUTIONS:
        w = sorted(x for rep in waits[(scenario, sol)] for _, x in rep)
        ax.plot(w, [(i + 1) / len(w) for i in range(len(w))], color=COLORS[sol], linewidth=2,
                marker=MARKERS[sol], markevery=len(w) // 8, markersize=5, label=LABELS[sol])
    ax.set_xlabel("czas oczekiwania [ms]")
    ax.set_ylabel("odsetek oczekiwań ≤ x")
    ax.set_xlim(left=0)
    ax.set_ylim(0, 1.01)
    ax.legend(frameon=False, fontsize=9, loc="lower right")
    style(ax)
    ax.grid(axis="x", color=GRID, linewidth=0.8)
    save(fig, out, name)


def wait_over_time(out, waits, duration, scenario, name):
    """Max wait in 5-second windows, one panel per solution, one line per repetition."""
    window = 5
    fig, axes = plt.subplots(1, len(SOLUTIONS), figsize=(7.0, 2.8), sharey=True)
    for ax, sol in zip(axes, SOLUTIONS):
        for rep in waits[(scenario, sol)]:
            buckets = {}
            for t, w in rep:
                k = int(t // (window * 1000))
                buckets[k] = max(buckets.get(k, 0), w)
            keys = sorted(k for k in buckets if (k + 1) * window <= duration)
            ax.plot([k * window for k in keys], [buckets[k] / 1000 for k in keys],
                    color=COLORS[sol], linewidth=1.2, alpha=0.75)
        ax.set_title(LABELS[sol], fontsize=10, color=TEXT)
        ax.set_xlabel("czas od startu [s]")
        style(ax)
    axes[0].set_ylim(bottom=0)  # shared axis: only after all the panels are drawn
    axes[0].set_ylabel("maks. oczekiwanie [s]")
    save(fig, out, name)


def throughput_panels(out, chosen, name):
    """Small multiples (separate y scales): mean throughput, a dot for every repetition."""
    fig, axes = plt.subplots(1, len(SCENARIOS), figsize=(7.0, 2.8))
    for ax, (key, sc) in zip(axes, SCENARIOS.items()):
        reps = [column(chosen[(key, s)], "throughput") for s in SOLUTIONS]
        means = [mean(r) for r in reps]
        bars = ax.bar(range(3), means, 0.7, color=[COLORS[s] for s in SOLUTIONS])
        top = max(max(r) for r in reps)
        for x, (b, m, r) in enumerate(zip(bars, means, reps)):
            ax.scatter([x] * len(r), r, s=9, color="white", edgecolor=TEXT, linewidth=0.7, zorder=3)
            ax.text(x, max(r) + top * 0.02, fmt(m, 1), ha="center", va="bottom", fontsize=8, color=TEXT)
        ax.set_title(sc["title"], fontsize=10, color=TEXT)
        ax.set_xticks(range(3), [LABELS[s] for s in SOLUTIONS])
        ax.set_ylim(0, top * 1.18)
        style(ax)
        ax.tick_params(axis="x", labelsize=7.5)
    axes[0].set_ylabel("posiłki / s")
    save(fig, out, name)


def table_rows(chosen, cells):
    """Rows of a table with one block per scenario; cells(runs, key, sol) gives the columns."""
    lines = []
    for key, sc in SCENARIOS.items():
        for i, sol in enumerate(SOLUTIONS):
            head = sc["title"] if i == 0 else ""
            lines.append(f"{head} & {LABELS[sol]} & " + " & ".join(cells(chosen[(key, sol)], key, sol)) + r" \\")
        lines.append(r"\midrule" if key != list(SCENARIOS)[-1] else r"\bottomrule")
    return lines


def summary_table(gen, chosen):
    def cells(runs, key, sol):
        return [fmt(mean(column(runs, "throughput")), 1), fmt(mean(column(runs, "mean_meals")), 1),
                f"{min(int(r['min_meals']) for r in runs)}", f"{max(int(r['max_meals']) for r in runs)}",
                fmt(mean(column(runs, "jain")), 3), fmt(mean(column(runs, "avg_eating")), 2)]
    lines = [r"\begin{tabular}{llrrrrrr}", r"\toprule",
             r"scenariusz & rozwiązanie & posiłki/s & posiłki na filozofa & min & max & Jain & jedzący \\",
             r"\midrule"]
    lines += table_rows(chosen, cells) + [r"\end{tabular}"]
    (gen / "summary.tex").write_text("\n".join(lines) + "\n")


def spread_table(gen, chosen):
    def cells(runs, key, sol):
        between = column(runs, "spread_pct")
        return [fmt(mean(between), 1), f"{fmt(min(between), 1)}-{fmt(max(between), 1)}",
                fmt(spread(column(runs, "throughput")), 1), fmt(spread(column(runs, "avg_wait_ms")), 1),
                fmt(spread(column(runs, "p99_wait_ms")), 1)]
    lines = [r"\begin{tabular}{llrrrrr}", r"\toprule",
             r" & & \multicolumn{2}{c}{między filozofami} & \multicolumn{3}{c}{między powtórzeniami} \\",
             r"\cmidrule(lr){3-4}\cmidrule(lr){5-7}",
             r"scenariusz & rozwiązanie & średnio & zakres & posiłki/s & śr. ocz. & p99 ocz. \\",
             r"\midrule"]
    lines += table_rows(chosen, cells) + [r"\end{tabular}"]
    (gen / "spread.tex").write_text("\n".join(lines) + "\n")


def wait_table(gen, chosen, waits):
    def cells(runs, key, sol):
        w = sorted(x for rep in waits[(key, sol)] for _, x in rep)
        values = [mean(w)] + [percentile(w, p) for p in (0.50, 0.95, 0.99)] + [w[-1]]
        return [fmt(v, 0) for v in values]
    lines = [r"\begin{tabular}{llrrrrr}", r"\toprule",
             r"scenariusz & rozwiązanie & średnia & p50 & p95 & p99 & maks. \\", r"\midrule"]
    lines += table_rows(chosen, cells) + [r"\end{tabular}"]
    (gen / "waits.tex").write_text("\n".join(lines) + "\n")


def philosophers_table(gen, phil, scenario, name):
    lines = [r"\begin{tabular}{l" + "r" * 15 + "}", r"\toprule",
             r" & \multicolumn{5}{c}{posiłki} & \multicolumn{5}{c}{śr. oczekiwanie [ms]}"
             r" & \multicolumn{5}{c}{maks. oczekiwanie [ms]} \\",
             r"\cmidrule(lr){2-6}\cmidrule(lr){7-11}\cmidrule(lr){12-16}",
             "filozof & " + " & ".join(["0", "1", "2", "3", "4"] * 3) + r" \\", r"\midrule"]
    for sol in SOLUTIONS:
        cells = [fmt(mean(v), 1) for v in zip(*per_philosopher(phil, scenario, sol, "meals"))]
        cells += [fmt(mean(v), 0) for v in zip(*per_philosopher(phil, scenario, sol, "avg_wait_ms"))]
        cells += [fmt(max(v), 0) for v in zip(*per_philosopher(phil, scenario, sol, "max_wait_ms"))]
        lines.append(f"{LABELS[sol]} & " + " & ".join(cells) + r" \\")
    lines += [r"\bottomrule", r"\end{tabular}"]
    (gen / f"{name}.tex").write_text("\n".join(lines) + "\n")


def deadlock_table(gen, runs):
    groups = {}
    for r in runs:
        if r["solution"] == "deadlock" and int(r["time_s"]) == SERIES["time_s"]:
            key = (int(r["n"]), int(r["pause_min_ms"]), int(r["pause_max_ms"]))
            groups.setdefault(key, []).append(r)
    lines = [r"\begin{tabular}{rlrrr}", r"\toprule",
             r"N & pauza [ms] & zakleszczenia & po czasie [s] & posiłki \\", r"\midrule"]
    for (n, lo, hi), rows in sorted(groups.items()):
        hits = [r for r in rows if r["deadlock"] == "1"]
        at = column(hits, "deadlock_at_s")
        when = low_high(at, fmt) if hits else "-"
        meals = low_high(column(rows, "total_meals"), int)
        lines.append(f"{n} & {lo}-{hi} & {len(hits)} z {len(rows)} & {when} & {meals} \\\\")
    lines += [r"\bottomrule", r"\end{tabular}"]
    (gen / "deadlock.tex").write_text("\n".join(lines) + "\n")


def print_numbers(chosen, phil, waits):
    """The numbers quoted in the text of the report."""
    for (key, sol), runs in sorted(chosen.items()):
        eat = sum(sum(float(r["total_eat_ms"]) for r in rows) for rows in phil[(key, sol)])
        meals = sum(sum(int(r["meals"]) for r in rows) for rows in phil[(key, sol)])
        print(f"{key:8}{sol:10} reps={len(runs)}"
              f" thr={[round(x, 2) for x in column(runs, 'throughput')]}"
              f" min/max={[(r['min_meals'], r['max_meals']) for r in runs]}"
              f" spread={[round(x, 1) for x in column(runs, 'spread_pct')]}"
              f" p99={[round(x) for x in column(runs, 'p99_wait_ms')]}"
              f" maxw={[round(x) for x in column(runs, 'max_wait_ms')]}"
              f" eat/meal={eat / meals:.1f}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--results", default="results/v2")
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

    phil, waits = {}, {}
    for key, group in chosen.items():
        phil[key] = [read_csv(res / f"{r['run_id']}_philosophers.csv") for r in group]
        waits[key] = [[(float(w["t_hungry_ms"]), float(w["wait_ms"]))
                       for w in read_csv(res / f"{r['run_id']}_waits.csv")] for r in group]

    plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 9})
    meals_bars(figs, phil, "n5", "meals_n5")
    meals_bars(figs, phil, "glutton", "meals_glutton")
    per_philosopher_lines(figs, phil, "n50", "meals", "liczba posiłków", "meals_n50")
    per_philosopher_lines(figs, phil, "n50", "avg_wait_ms", "śr. oczekiwanie [ms]", "avg_wait_n50")
    wait_cdf(figs, waits, "n5", "wait_cdf_n5")
    wait_cdf(figs, waits, "n50", "wait_cdf_n50")
    wait_over_time(figs, waits, SERIES["time_s"], "n50", "wait_over_time_n50")
    throughput_panels(figs, chosen, "throughput")

    summary_table(gen, chosen)
    spread_table(gen, chosen)
    wait_table(gen, chosen, waits)
    philosophers_table(gen, phil, "n5", "philosophers_n5")
    philosophers_table(gen, phil, "glutton", "philosophers_glutton")
    deadlock_table(gen, runs)
    print_numbers(chosen, phil, waits)
    print(f"figures -> {figs}, tables -> {gen}")


if __name__ == "__main__":
    main()
