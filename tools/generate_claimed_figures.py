#!/usr/bin/env python3
import argparse
import math
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

FAMILIES = ["random", "erdos_renyi", "barabasi_albert", "grid", "road"]
FAMILY_LABELS = {
    "random": "Random",
    "erdos_renyi": "Erdős–Rényi",
    "barabasi_albert": "Barabási–Albert",
    "grid": "Grid",
    "road": "Road-like",
}
FAMILY_COLOURS = {
    "random": "#E53935",
    "erdos_renyi": "#8E24AA",
    "barabasi_albert": "#00897B",
    "grid": "#F4511E",
    "road": "#3949AB",
}


def add_derived_columns(df: pd.DataFrame) -> pd.DataFrame:
    out = df.copy()
    out["reachable_frac"] = out["reachable"] / out["size"]
    out["is_connectivity_outlier"] = out["reachable_frac"] < 0.90
    out["bmssp_over_fib"] = out["bmssp_ms"] / out["dijkstra_fib_ms"]
    out["fib_over_binary"] = out["dijkstra_fib_ms"] / out["dijkstra_ms"]
    out["c2_over_c1"] = out["bmssp_ms"] / out["dijkstra_fib_ms"]
    out["stale_frac"] = out["binpq_stale_pop_count"] / (out["binpq_pop_count"] + 1)
    out["dk_rate"] = out["fib_decrease_key_count"] / (out["fib_extract_count"] + 1)
    return out


def save(fig, outdir: str, name: str):
    path = os.path.join(outdir, name)
    fig.savefig(path, bbox_inches="tight", dpi=170)
    plt.close(fig)
    print("Wrote", path)


def plot_scale_runtime_types(df: pd.DataFrame, outdir: str):
    for gt in FAMILIES:
        sub = df[df["graph_type"] == gt].copy().sort_values(["size", "param"])
        agg = sub.groupby("size")[["dijkstra_ms", "dijkstra_fib_ms", "bmssp_ms"]].mean().reset_index()

        fig, ax = plt.subplots(figsize=(7.2, 4.8))
        ax.plot(agg["size"], agg["dijkstra_ms"], marker="o", label="Binary Dijkstra")
        ax.plot(agg["size"], agg["dijkstra_fib_ms"], marker="o", label="Fibonacci Dijkstra")
        ax.plot(agg["size"], agg["bmssp_ms"], marker="o", label="BMSSP")
        ax.set_xscale("log")
        ax.set_yscale("log")
        ax.set_xlabel("Graph size (nodes)")
        ax.set_ylabel("Time (ms)")
        ax.set_title(f"Runtime vs size: {FAMILY_LABELS[gt]}")
        ax.grid(True, which="both", ls="--", lw=0.4, alpha=0.6)
        ax.legend(framealpha=0.8)
        fig.tight_layout()
        save(fig, outdir, f"scale_runtime_type_{gt}.png")


def plot_scale_speedup_bmssp_fib(df: pd.DataFrame, outdir: str):
    fig, ax = plt.subplots(figsize=(8.0, 5.0))
    for gt in FAMILIES:
        sub = df[df["graph_type"] == gt].copy().sort_values(["size", "param"])
        agg = sub.groupby("size")["bmssp_vs_fib_speedup"].mean().reset_index()
        ax.plot(agg["size"], agg["bmssp_vs_fib_speedup"], marker="o", label=FAMILY_LABELS[gt], color=FAMILY_COLOURS[gt])
    ax.axhline(1.0, color="black", linestyle=":", linewidth=1.2)
    ax.set_xscale("log")
    ax.set_xlabel("Graph size (nodes)")
    ax.set_ylabel("Runtime ratio (fib / BMSSP)")
    ax.set_title("BMSSP vs Fibonacci Dijkstra")
    ax.grid(True, which="both", ls="--", lw=0.4, alpha=0.6)
    ax.legend(framealpha=0.8)
    fig.tight_layout()
    save(fig, outdir, "scale_speedup_bmssp_fib.png")


def plot_pq_telemetry(df: pd.DataFrame, outdir: str):
    clean = df[~df["is_connectivity_outlier"]]
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 5.2))

    labels = [FAMILY_LABELS[f] for f in FAMILIES]
    colours = [FAMILY_COLOURS[f] for f in FAMILIES]
    stale = [clean[clean["graph_type"] == gt]["stale_frac"].mean() for gt in FAMILIES]
    dkr = [clean[clean["graph_type"] == gt]["dk_rate"].mean() for gt in FAMILIES]

    b1 = ax1.bar(labels, stale, color=colours, alpha=0.82)
    ax1.bar_label(b1, fmt="%.2f", fontsize=8)
    ax1.set_ylabel("Stale pops / total pops")
    ax1.set_title("Binary heap stale-pop fraction")
    ax1.tick_params(axis="x", rotation=15)
    ax1.grid(axis="y", ls="--", lw=0.4, alpha=0.6)

    b2 = ax2.bar(labels, dkr, color=colours, alpha=0.82)
    ax2.bar_label(b2, fmt="%.2f", fontsize=8)
    ax2.set_ylabel("Decrease-key operations / extract")
    ax2.set_title("Fibonacci decrease-key rate")
    ax2.tick_params(axis="x", rotation=15)
    ax2.grid(axis="y", ls="--", lw=0.4, alpha=0.6)

    fig.suptitle("Priority queue telemetry by graph family", fontsize=11.5)
    fig.tight_layout()
    save(fig, outdir, "pq_telemetry.png")


def plot_stale_vs_fib_gap(df: pd.DataFrame, outdir: str):
    clean = df[~df["is_connectivity_outlier"]]
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 5.2))

    for gt in FAMILIES:
        sub = clean[clean["graph_type"] == gt]
        c = FAMILY_COLOURS[gt]
        lbl = FAMILY_LABELS[gt]
        ax1.scatter(sub["stale_frac"], sub["fib_over_binary"], s=50, alpha=0.8, color=c, label=lbl)
        ax2.scatter(sub["dk_rate"], sub["fib_over_binary"], s=50, alpha=0.8, color=c, label=lbl)

    for ax, xcol, xlabel in [
        (ax1, "stale_frac", "Binary stale-pop fraction"),
        (ax2, "dk_rate", "Fib decrease-key rate"),
    ]:
        x = clean[xcol].values
        y = clean["fib_over_binary"].values
        if len(x) >= 2:
            m, b = np.polyfit(x, y, 1)
            xx = np.linspace(x.min(), x.max(), 100)
            ax.plot(xx, m * xx + b, "k--", linewidth=1.2)
        ax.axhline(1.0, color="firebrick", ls=":", lw=1.0)
        ax.set_xlabel(xlabel)
        ax.set_ylabel("Fibonacci / Binary runtime ratio")
        ax.grid(True, ls="--", lw=0.4, alpha=0.6)

    ax1.legend(fontsize=8, framealpha=0.8)
    fig.suptitle("Queue churn vs Fibonacci/Binary gap", fontsize=11.5)
    fig.tight_layout()
    save(fig, outdir, "stale_vs_fib_gap.png")


def plot_c2c1_stability(df: pd.DataFrame, outdir: str):
    clean = df[~df["is_connectivity_outlier"]]
    fig, ax = plt.subplots(figsize=(10, 5.5))
    for gt in FAMILIES:
        sub = clean[clean["graph_type"] == gt]
        agg = sub.groupby("size")["c2_over_c1"].mean().reset_index()
        x = agg["size"].values
        y = agg["c2_over_c1"].values
        c = FAMILY_COLOURS[gt]
        ax.plot(x, y, marker="o", color=c, label=FAMILY_LABELS[gt], linewidth=1.8)
        if len(x) >= 2:
            m, b = np.polyfit(np.log(x), y, 1)
            xx = np.array([x.min(), x.max()])
            ax.plot(xx, m * np.log(xx) + b, linestyle="--", color=c, alpha=0.65)
    ax.axhline(1.0, color="black", linestyle=":", linewidth=1.0)
    ax.set_xscale("log")
    ax.set_xlabel("Graph size n (nodes)")
    ax.set_ylabel("c2/c1 (BMSSP / Fibonacci runtime)")
    ax.set_title("Stability of c2/c1 across graph sizes")
    ax.grid(True, which="both", ls="--", lw=0.4, alpha=0.6)
    ax.legend(framealpha=0.85)
    fig.tight_layout()
    save(fig, outdir, "c2c1_stability.png")


def plot_bmssp_parameter_regime(outdir: str):
    n_range = np.logspace(3, 10, 300)
    k = [max(1, int(np.log2(n) ** (1 / 3))) for n in n_range]
    t = [max(1, int(np.log2(n) ** (2 / 3))) for n in n_range]
    l = [max(1, int(math.ceil(np.log2(n) / max(1, int(np.log2(n) ** (2 / 3)))))) for n in n_range]
    batch = [2 ** (max(1, int(np.log2(n) ** (2 / 3))) - 1) for n in n_range]

    fig, axes = plt.subplots(2, 2, figsize=(13, 8))
    plots = [
        (axes[0, 0], k, "k", "k = floor((log2 n)^(1/3))", "#E53935"),
        (axes[0, 1], t, "t", "t = floor((log2 n)^(2/3))", "#2196F3"),
        (axes[1, 0], l, "ℓ", "ℓ = ceil(log2 n / t)", "#4CAF50"),
        (axes[1, 1], batch, "2^(t-1)", "Batch size", "#FF9800"),
    ]
    for ax, vals, yl, ttl, c in plots:
        ax.plot(n_range, vals, color=c, linewidth=2.0)
        ax.set_xscale("log")
        ax.set_xlabel("n")
        ax.set_ylabel(yl)
        ax.set_title(ttl, fontsize=10)
        ax.grid(True, which="both", ls="--", lw=0.4, alpha=0.6)
    fig.suptitle("BMSSP parameter regime across graph sizes", fontsize=12)
    fig.tight_layout()
    save(fig, outdir, "bmssp_parameter_regime.png")


def plot_structural_correlates(df: pd.DataFrame, outdir: str):
    clean = df[~df["is_connectivity_outlier"]]
    props = [
        ("graph_avg_distance_hops_unweighted", "Average hop distance"),
        ("graph_degree_gini", "Degree Gini"),
        ("graph_avg_clustering_coefficient", "Avg clustering"),
    ]
    fig, axes = plt.subplots(1, 3, figsize=(15, 5.2))
    for ax, (col, xlabel) in zip(axes, props):
        for gt in FAMILIES:
            sub = clean[clean["graph_type"] == gt]
            ax.scatter(sub[col], sub["bmssp_over_fib"], s=50, alpha=0.8, color=FAMILY_COLOURS[gt], label=FAMILY_LABELS[gt])
        x = clean[col].values
        y = clean["bmssp_over_fib"].values
        if len(x) >= 2:
            m, b = np.polyfit(x, y, 1)
            xx = np.linspace(x.min(), x.max(), 100)
            ax.plot(xx, m * xx + b, "k--", linewidth=1.2)
        ax.axhline(1.0, color="firebrick", ls=":", lw=1.0)
        ax.set_xlabel(xlabel)
        ax.set_ylabel("BMSSP / Fibonacci ratio")
        ax.grid(True, ls="--", lw=0.4, alpha=0.6)
    axes[0].legend(fontsize=7.5, framealpha=0.8)
    fig.suptitle("Structural correlates of BMSSP/Fibonacci performance", fontsize=11.5)
    fig.tight_layout()
    save(fig, outdir, "structural_correlates.png")


def plot_crossover_projection(df: pd.DataFrame, outdir: str):
    clean = df[(~df["is_connectivity_outlier"]) & (df["size"] >= 10000)]
    n_ref = 50000
    n_vals = np.logspace(3, 14, 500)
    fig, ax = plt.subplots(figsize=(11, 6))
    fib_curve = n_vals * np.log(n_vals) / (n_ref * np.log(n_ref))
    for gt in FAMILIES:
        sub = clean[clean["graph_type"] == gt]
        ratio = (sub["bmssp_ms"] / sub["dijkstra_fib_ms"]).mean()
        bmssp_curve = ratio * n_vals * (np.log(n_vals) ** (2 / 3)) / (n_ref * np.log(n_ref))
        ax.plot(n_vals, bmssp_curve, color=FAMILY_COLOURS[gt], linewidth=1.8, label=f"{FAMILY_LABELS[gt]} (c2/c1≈{ratio:.2f})")
    ax.plot(n_vals, fib_curve, color="black", linestyle="--", linewidth=2.0, label="Fibonacci Dijkstra")
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("Graph size n (nodes)")
    ax.set_ylabel("Normalised runtime")
    ax.set_title("Theoretical crossover projection")
    ax.grid(True, which="both", ls="--", lw=0.4, alpha=0.6)
    ax.legend(fontsize=8.5, framealpha=0.85)
    fig.tight_layout()
    save(fig, outdir, "crossover_projection.png")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--csv", required=True)
    parser.add_argument("--outdir", required=True)
    args = parser.parse_args()

    os.makedirs(args.outdir, exist_ok=True)
    df = pd.read_csv(args.csv)
    df["size"] = df["size"].astype(int)
    df = add_derived_columns(df)

    plot_scale_runtime_types(df, args.outdir)
    plot_scale_speedup_bmssp_fib(df, args.outdir)
    plot_pq_telemetry(df, args.outdir)
    plot_stale_vs_fib_gap(df, args.outdir)
    plot_c2c1_stability(df, args.outdir)
    plot_bmssp_parameter_regime(args.outdir)
    plot_structural_correlates(df, args.outdir)
    plot_crossover_projection(df, args.outdir)


if __name__ == "__main__":
    main()
