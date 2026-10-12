"""Plots and summary tables for the Boyer-Moore parallelism analysis.

Usage: python scripts/plot_results.py [results/bm_results.csv] [results/plots]

Reads the CSV written by bm_bench and produces:
  time_vs_size.png, throughput.png, speedup_vs_threads.png,
  efficiency_vs_threads.png, overhead_breakdown.png, crossover.png,
  amdahl.png, granularity.png and summary.md (the numbers quoted in the report).
Follow-up results from scripts/run_experiments.sh (experiments.csv, membw.csv,
frequency.csv next to the main CSV) are included when present.
"""

import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

CSV = Path(sys.argv[1] if len(sys.argv) > 1 else "results/bm_results.csv")
OUT = Path(sys.argv[2] if len(sys.argv) > 2 else "results/plots")
OUT.mkdir(parents=True, exist_ok=True)

# Main dataset first; later datasets only add sizes beyond what earlier ones cover
DATASET_ORDER = ["human", "wheat", "ecoli"]
PATTERN_LABEL = {4: "short pattern (GATC, 4 chars)", 32: "long pattern (32 chars)"}


def human_size(n):
    for unit, factor in (("GB", 1e9), ("MB", 1e6), ("KB", 1e3)):
        if n >= factor:
            value = n / factor
            return f"{value:.1f} {unit}" if value < 10 and value != int(value) else f"{value:.0f} {unit}"
    return f"{n} B"


def load():
    df = pd.read_csv(CSV)
    df["plen"] = df["pattern"].str.len()
    seq = df[df.algo == "sequential"][["dataset", "pattern", "size_bytes", "median_s"]]
    seq = seq.rename(columns={"median_s": "seq_s"})
    par = df[df.algo == "parallel"].merge(seq, on=["dataset", "pattern", "size_bytes"])
    par["speedup"] = par.seq_s / par.median_s
    par["speedup_cold"] = par.seq_s / (par.median_s + par.t_pool)
    par["efficiency"] = par.speedup / par.threads
    return df, par


def combined(frame):
    """One series across datasets: each size taken from the first dataset that has it."""
    order = {d: i for i, d in enumerate(DATASET_ORDER)}
    frame = frame.assign(rank=frame.dataset.map(lambda d: order.get(d, 99)))
    keep = frame[frame.dataset != "ecoli"] if (frame.dataset != "ecoli").any() else frame
    keep = keep.sort_values("rank").drop_duplicates(["pattern", "size_bytes", "threads"], keep="first")
    return keep.sort_values("size_bytes")


def pattern_axes(n_patterns, title, sharey=True):
    fig, axes = plt.subplots(1, n_patterns, figsize=(6.5 * n_patterns, 4.8), sharey=sharey, squeeze=False)
    fig.suptitle(title)
    return fig, axes[0]


def plot_time_vs_size(df, patterns):
    fig, axes = pattern_axes(len(patterns), "Execution time vs input size (median of repetitions)")
    data = combined(df)
    for ax, plen in zip(axes, patterns):
        d = data[data.plen == plen]
        s = d[d.algo == "sequential"]
        ax.plot(s.size_bytes, s.median_s, "k-o", lw=2.2, ms=4, label="sequential")
        for p in (2, 4, 8, 24, 48):
            q = d[(d.algo == "parallel") & (d.threads == p)]
            if len(q):
                ax.plot(q.size_bytes, q.median_s, "-o", ms=3, label=f"{p} threads")
        ax.set_xscale("log")
        ax.set_yscale("log")
        ax.set_xlabel("input size (bytes)")
        ax.set_title(PATTERN_LABEL.get(plen, f"{plen} chars"))
        ax.grid(True, which="both", alpha=0.3)
    axes[0].set_ylabel("time (s)")
    axes[-1].legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(OUT / "time_vs_size.png", dpi=150)
    plt.close(fig)


def plot_throughput(df, patterns):
    fig, axes = pattern_axes(len(patterns), "Throughput vs input size")
    data = combined(df)
    for ax, plen in zip(axes, patterns):
        d = data[data.plen == plen]
        s = d[d.algo == "sequential"]
        ax.plot(s.size_bytes, s.size_bytes / s.median_s / 1e9, "k-o", lw=2.2, ms=4, label="sequential")
        for p in (4, 8, 16, 24, 48):
            q = d[(d.algo == "parallel") & (d.threads == p)]
            if len(q):
                ax.plot(q.size_bytes, q.size_bytes / q.median_s / 1e9, "-o", ms=3, label=f"{p} threads")
        ax.set_xscale("log")
        ax.set_xlabel("input size (bytes)")
        ax.set_title(PATTERN_LABEL.get(plen, f"{plen} chars"))
        ax.grid(True, which="both", alpha=0.3)
    axes[0].set_ylabel("throughput (GB/s)")
    axes[-1].legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(OUT / "throughput.png", dpi=150)
    plt.close(fig)


def representative_sizes(par):
    sizes = sorted(par.size_bytes.unique())
    wanted = [1e3, 1e5, 1e6, 1e7, 1e8, 1e9]
    chosen = [s for s in sizes if any(abs(s - w) / w < 0.01 for w in wanted)]
    chosen += [s for s in sizes if s > 1e9]
    return chosen


def plot_scaling(par, patterns, column, filename, title, ylabel, ideal):
    fig, axes = pattern_axes(len(patterns), title)
    data = combined(par)
    sizes = representative_sizes(data)
    cmap = plt.get_cmap("viridis", max(len(sizes), 2))
    for ax, plen in zip(axes, patterns):
        d = data[data.plen == plen]
        threads = sorted(d.threads.unique())
        if ideal:
            ax.plot(threads, threads, "k:", label="ideal (linear)")
        ax.axhline(1.0, color="red", lw=1, ls="--", label="sequential = 1")
        for i, size in enumerate(sizes):
            q = d[d.size_bytes == size].sort_values("threads")
            if len(q):
                ax.plot(q.threads, q[column], "-o", ms=3, color=cmap(i),
                        label=f"{human_size(size)} ({q.dataset.iloc[0]})")
        ax.axvline(24, color="grey", lw=1, ls=":")
        ax.set_xlabel("threads (24 = number of cores)")
        ax.set_title(PATTERN_LABEL.get(plen, f"{plen} chars"))
        ax.grid(True, alpha=0.3)
    axes[0].set_ylabel(ylabel)
    axes[-1].legend(fontsize=7)
    fig.tight_layout()
    fig.savefig(OUT / filename, dpi=150)
    plt.close(fig)


def plot_overhead(par, patterns, p=24):
    fig, axes = pattern_axes(len(patterns), f"Where the time goes ({p} threads, share of total)", sharey=True)
    data = combined(par)
    phases = [("t_pool", "pool create+destroy"), ("t_pre", "preprocessing"),
              ("t_dispatch", "chunking + dispatch"), ("t_search", "parallel search"),
              ("t_merge", "merge (concat+sort+dedupe)")]
    colors = ["#bbbbbb", "#8c6bb1", "#e6550d", "#3182bd", "#31a354"]
    for ax, plen in zip(axes, patterns):
        d = data[(data.plen == plen) & (data.threads == p)].sort_values("size_bytes")
        d = d[d.size_bytes.isin(representative_sizes(data))]
        values = d[[c for c, _ in phases]].to_numpy()
        share = values / values.sum(axis=1, keepdims=True) * 100
        bottom = np.zeros(len(d))
        labels = [human_size(s) for s in d.size_bytes]
        for j, (_, name) in enumerate(phases):
            ax.bar(labels, share[:, j], bottom=bottom, color=colors[j], label=name)
            bottom += share[:, j]
        ax.set_title(PATTERN_LABEL.get(plen, f"{plen} chars"))
        ax.set_xlabel("input size")
        ax.tick_params(axis="x", rotation=45)
    axes[0].set_ylabel("% of time")
    handles, names = axes[0].get_legend_handles_labels()
    fig.legend(handles, names, loc="lower center", ncol=len(phases), fontsize=8)
    fig.tight_layout(rect=(0, 0.07, 1, 1))
    fig.savefig(OUT / "overhead_breakdown.png", dpi=150)
    plt.close(fig)


def crossover_table(par):
    """Input size from which parallel stays faster than sequential.

    The measured crossover is interpolated (log-log) between the two sizes where
    parallel goes from slower to faster. The model crossover uses section 4 of the
    report: n = r * T_fixed * p / (p - 1), with r the sequential scan rate at 1 GB
    and T_fixed the parallel time at the smallest inputs (pure overhead).
    """
    rows = []
    data = combined(par)
    for (plen, p), g in data.groupby(["plen", "threads"]):
        g = g.sort_values("size_bytes")
        for col, label in (("speedup", "warm pool"), ("speedup_cold", "incl. pool creation")):
            sizes = g.size_bytes.to_numpy(float)
            gap = np.log(g[col].to_numpy())  # > 0 means parallel is faster
            size = None
            for i in range(len(gap)):
                if (gap[i:] > 0).all():
                    if i == 0:
                        size = sizes[0]
                    else:
                        x0, x1 = np.log(sizes[i - 1]), np.log(sizes[i])
                        size = float(np.exp(x0 + (x1 - x0) * (-gap[i - 1]) / (gap[i] - gap[i - 1])))
                    break
            rows.append({"plen": plen, "threads": p, "mode": label, "crossover": size})
        if p > 1:
            at_1gb = g[(g.size_bytes - 1e9).abs() < 1e7]
            tiny = g[g.size_bytes <= 10000]
            if len(at_1gb) and len(tiny):
                rate = 1e9 / at_1gb.seq_s.iloc[0]
                fixed = tiny.median_s.median()
                rows.append({"plen": plen, "threads": p, "mode": "cost model",
                             "crossover": rate * fixed * p / (p - 1)})
    return pd.DataFrame(rows)


def plot_crossover(cross, patterns):
    fig, axes = pattern_axes(len(patterns), "Crossover: input size from which parallel beats sequential")
    for ax, plen in zip(axes, patterns):
        for mode, style in (("warm pool", "-o"), ("incl. pool creation", "--s"), ("cost model", ":^")):
            c = cross[(cross.plen == plen) & (cross["mode"] == mode) & (cross.threads > 1)].dropna()
            ax.plot(c.threads, c.crossover, style, label=mode)
        ax.set_yscale("log")
        ax.set_xlabel("threads")
        ax.set_title(PATTERN_LABEL.get(plen, f"{plen} chars"))
        ax.grid(True, which="both", alpha=0.3)
    axes[0].set_ylabel("crossover input size (bytes)")
    axes[-1].legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(OUT / "crossover.png", dpi=150)
    plt.close(fig)


def amdahl(p, f):
    return 1.0 / (f + (1.0 - f) / p)


def fit_serial_fraction(threads, speedup):
    grid = np.linspace(0.0, 1.0, 10001)
    errors = [np.sum((amdahl(threads, f) - speedup) ** 2) for f in grid]
    return grid[int(np.argmin(errors))]


def plot_amdahl(par, patterns):
    fig, axes = pattern_axes(len(patterns), "Measured speedup at the largest inputs vs Amdahl's law fit")
    fits = []
    data = par[par.threads <= 24]
    for ax, plen in zip(axes, patterns):
        for ds in DATASET_ORDER:
            d = data[(data.plen == plen) & (data.dataset == ds)]
            if d.empty:
                continue
            size = d.size_bytes.max()
            q = d[d.size_bytes == size].sort_values("threads")
            f = fit_serial_fraction(q.threads.to_numpy(float), q.speedup.to_numpy())
            fits.append({"plen": plen, "dataset": ds, "size": size, "serial_fraction": f,
                         "max_speedup": 1 / f if f > 0 else float("inf")})
            line = ax.plot(q.threads, q.speedup, "o", label=f"{ds} {human_size(size)} measured")
            xs = np.linspace(1, 24, 100)
            ax.plot(xs, amdahl(xs, f), "-", color=line[0].get_color(), alpha=0.7,
                    label=f"Amdahl fit, serial fraction = {f:.3f}")
        ax.plot([1, 24], [1, 24], "k:", label="ideal")
        ax.set_xlabel("threads")
        ax.set_title(PATTERN_LABEL.get(plen, f"{plen} chars"))
        ax.grid(True, alpha=0.3)
    axes[0].set_ylabel("speedup")
    axes[-1].legend(fontsize=7)
    fig.tight_layout()
    fig.savefig(OUT / "amdahl.png", dpi=150)
    plt.close(fig)
    return pd.DataFrame(fits)


def write_summary(df, par, cross, fits):
    lines = ["# Benchmark summary (generated by scripts/plot_results.py)", ""]
    lines.append("## Datasets and correctness")
    for ds, g in df.groupby("dataset"):
        lines.append(f"- {ds}: {human_size(g.size_bytes.max())} of sequence, "
                     f"{len(g)} rows, all correct: {bool(g.correct.all())}")
    lines.append("")

    data = combined(par)
    for plen in sorted(par.plen.unique()):
        d = data[data.plen == plen]
        lines.append(f"## {PATTERN_LABEL.get(plen, plen)}")
        lines.append("")
        lines.append("| size | dataset | sequential (s) | best threads | best time (s) | best speedup | speedup @24 | efficiency @24 | matches |")
        lines.append("|---|---|---|---|---|---|---|---|---|")
        for size, g in d.groupby("size_bytes"):
            best = g.loc[g.median_s.idxmin()]
            at24 = g[g.threads == 24]
            s24 = at24.speedup.iloc[0] if len(at24) else float("nan")
            e24 = at24.efficiency.iloc[0] if len(at24) else float("nan")
            lines.append(f"| {human_size(size)} | {best.dataset} | {best.seq_s:.6g} | {int(best.threads)} | "
                         f"{best.median_s:.6g} | {best.speedup:.2f} | {s24:.2f} | {e24:.2f} | {int(best.matches)} |")
        lines.append("")
        lines.append("Crossover (size from which parallel stays faster, interpolated):")
        lines.append("")
        lines.append("| threads | warm pool | incl. pool creation | cost model (warm pool) |")
        lines.append("|---|---|---|---|")
        c = cross[cross.plen == plen]
        for p, g in c.groupby("threads"):
            if p == 1:
                continue
            warm = g[g["mode"] == "warm pool"].crossover.iloc[0]
            cold = g[g["mode"] == "incl. pool creation"].crossover.iloc[0]
            model = g[g["mode"] == "cost model"].crossover
            model = model.iloc[0] if len(model) else None
            fmt = lambda v: human_size(int(v)) if v is not None and pd.notna(v) else "never"
            lines.append(f"| {p} | {fmt(warm)} | {fmt(cold)} | {fmt(model)} |")
        lines.append("")
        lines.append("Phase breakdown at 24 threads (median seconds):")
        lines.append("")
        lines.append("| size | total | pool | preprocess | dispatch | search | merge | of which sort |")
        lines.append("|---|---|---|---|---|---|---|---|")
        for _, r in d[d.threads == 24].sort_values("size_bytes").iterrows():
            lines.append(f"| {human_size(r.size_bytes)} | {r.median_s:.3g} | {r.t_pool:.3g} | {r.t_pre:.3g} | "
                         f"{r.t_dispatch:.3g} | {r.t_search:.3g} | {r.t_merge:.3g} | {r.t_sort:.3g} |")
        lines.append("")
        one = d[d.threads == 1][["size_bytes", "seq_s", "median_s"]]
        if len(one):
            lines.append("Pool overhead with 1 thread (parallel path vs plain sequential):")
            lines.append("")
            lines.append("| size | sequential (s) | 1-thread pool (s) | ratio |")
            lines.append("|---|---|---|---|")
            for _, r in one.sort_values("size_bytes").iterrows():
                lines.append(f"| {human_size(r.size_bytes)} | {r.seq_s:.3g} | {r.median_s:.3g} | {r.median_s / r.seq_s:.2f} |")
            lines.append("")

    pools = df[df.algo == "parallel"].groupby("threads").t_pool.median()
    lines.append("## Thread pool create+destroy cost (median over all runs)")
    lines.append("")
    lines.append("| threads | seconds |")
    lines.append("|---|---|")
    for threads, cost in pools.items():
        lines.append(f"| {threads} | {cost:.3g} |")
    lines.append("")

    if len(fits):
        lines.append("## Amdahl fit at the largest input (1-24 threads)")
        lines.append("")
        lines.append("| pattern | dataset | size | serial fraction | implied max speedup |")
        lines.append("|---|---|---|---|---|")
        for _, r in fits.iterrows():
            lines.append(f"| {r.plen} chars | {r.dataset} | {human_size(r['size'])} | {r.serial_fraction:.3f} | {r.max_speedup:.1f} |")
        lines.append("")

    lines += experiments_section()
    (OUT.parent / "summary.md").write_text("\n".join(lines))


def experiments_section():
    """Plot and tables for the follow-up experiments (scripts/run_experiments.sh), if present."""
    exp_path = CSV.parent / "experiments.csv"
    if not exp_path.exists():
        return []
    ex = pd.read_csv(exp_path)
    ex["plen"] = ex.pattern.str.len()
    ex["k"] = ex.algo.map(lambda a: 1 if a in ("parallel", "sequential") else int(a.split("_x")[1]))
    seq = ex[ex.algo == "sequential"].groupby(["dataset", "plen", "size_bytes"]).median_s.median()
    par = ex[ex.algo != "sequential"].copy()
    par["seq_s"] = [seq[key] for key in zip(par.dataset, par.plen, par.size_bytes)]
    par["speedup"] = par.seq_s / par.median_s

    lines = ["## Follow-up experiments", ""]
    lines.append("All rows correct: " + str(bool(ex.correct.all())))
    lines.append("")

    cores = par[par.dataset.isin(["P-cores", "E-cores"])]
    if len(cores):
        lines.append("Scan on one core type only (1 GB, threads pinned with taskset):")
        lines.append("")
        lines.append("| cores | pattern | sequential (s) | threads | parallel (s) | speedup vs own sequential |")
        lines.append("|---|---|---|---|---|---|")
        for _, r in cores.sort_values(["plen", "dataset", "threads"]).iterrows():
            lines.append(f"| {r.dataset} | {r.plen} chars | {r.seq_s:.3f} | {r.threads} | {r.median_s:.3f} | {r.speedup:.2f} |")
        lines.append("")

    gran = par[par.dataset.isin(["human", "wheat"])].sort_values(["dataset", "plen", "size_bytes", "k"])
    if len(gran):
        lines.append("Chunks per thread at 24 threads:")
        lines.append("")
        lines.append("| dataset | pattern | size | chunks/thread | time (s) | search (s) | merge (s) | speedup |")
        lines.append("|---|---|---|---|---|---|---|---|")
        for _, r in gran.iterrows():
            lines.append(f"| {r.dataset} | {r.plen} chars | {human_size(r.size_bytes)} | {r.k} | {r.median_s:.3f} | "
                         f"{r.t_search:.3f} | {r.t_merge:.3f} | {r.speedup:.2f} |")
        lines.append("")

        fig, axes = pattern_axes(2, "Speedup at 24 threads vs number of chunks per thread")
        for ax, plen in zip(axes, (4, 32)):
            for (ds, size), g in gran[gran.plen == plen].groupby(["dataset", "size_bytes"]):
                ax.plot(g.k, g.speedup, "-o", label=f"{ds} {human_size(size)}")
            ax.set_xscale("log", base=2)
            ax.set_xlabel("chunks per thread (1 = original design)")
            ax.set_title(PATTERN_LABEL.get(plen, f"{plen} chars"))
            ax.grid(True, alpha=0.3)
        axes[0].set_ylabel("speedup over sequential")
        axes[-1].legend(fontsize=8)
        fig.tight_layout()
        fig.savefig(OUT / "granularity.png", dpi=150)
        plt.close(fig)

    for name, title in (("membw.csv", "Memory bandwidth (summing a 4 GB array)"),
                        ("frequency.csv", "Clock frequency while cores are busy")):
        path = CSV.parent / name
        if path.exists():
            table = pd.read_csv(path)
            lines.append(f"{title}:")
            lines.append("")
            lines.append("| " + " | ".join(table.columns) + " |")
            lines.append("|" + "---|" * len(table.columns))
            for row in table.itertuples(index=False):
                lines.append("| " + " | ".join(f"{v:.1f}" if isinstance(v, float) else str(v) for v in row) + " |")
            lines.append("")
    return lines


def main():
    df, par = load()
    patterns = sorted(df.plen.unique())
    plot_time_vs_size(df, patterns)
    plot_throughput(df, patterns)
    plot_scaling(par, patterns, "speedup", "speedup_vs_threads.png",
                 "Speedup over sequential vs threads", "speedup (sequential / parallel)", ideal=True)
    plot_scaling(par, patterns, "efficiency", "efficiency_vs_threads.png",
                 "Parallel efficiency vs threads", "efficiency (speedup / threads)", ideal=False)
    plot_overhead(par, patterns)
    cross = crossover_table(par)
    plot_crossover(cross, patterns)
    fits = plot_amdahl(par, patterns)
    write_summary(df, par, cross, fits)
    print(f"Wrote plots to {OUT} and summary to {OUT.parent / 'summary.md'}")


if __name__ == "__main__":
    main()
