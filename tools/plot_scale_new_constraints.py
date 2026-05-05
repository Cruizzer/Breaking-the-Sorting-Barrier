#!/usr/bin/env python3
"""Generate updated benchmark figures using the strengthened benchmark methodology.

Methodology assumptions expected in CSV:
- warmup_runs >= 1
- order_randomized == 1
- trials >= 10
- median/std/p95 timing columns are present
- BMSSP preparation-inclusive timing columns are present
"""

from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

FAMILIES = ["random", "erdos_renyi", "barabasi_albert", "grid", "road"]
FAMILY_LABELS = {
    "random": "Random",
    "erdos_renyi": "Erdos-Renyi",
    "barabasi_albert": "Barabasi-Albert",
    "grid": "Grid",
    "road": "Road-like",
}
COLORS = {
    "dijkstra": "#1f77b4",
    "fib": "#ff7f0e",
    "bmssp": "#2ca02c",
    "bmssp_total": "#d62728",
}


def _require_columns(df: pd.DataFrame, cols: list[str]) -> None:
    missing = [c for c in cols if c not in df.columns]
    if missing:
        raise ValueError(f"Missing required columns: {missing}")


def _load(csv_path: Path) -> pd.DataFrame:
    df = pd.read_csv(csv_path)
    _require_columns(
        df,
        [
            "graph_type", "size", "trials", "warmup_runs", "order_randomized",
            "dijkstra_ms", "dijkstra_median_ms", "dijkstra_std_ms", "dijkstra_p95_ms",
            "dijkstra_fib_ms", "dijkstra_fib_median_ms", "dijkstra_fib_std_ms", "dijkstra_fib_p95_ms",
            "bmssp_ms", "bmssp_median_ms", "bmssp_std_ms", "bmssp_p95_ms",
            "bmssp_total_with_cd_ms", "bmssp_total_with_cd_median_ms", "bmssp_total_with_cd_std_ms", "bmssp_total_with_cd_p95_ms",
            "match_count_binary_fib", "match_count_binary_bmssp", "match_count_fib_bmssp",
            "bmssp_vs_binary_speedup", "bmssp_vs_fib_speedup", "fib_vs_binary_speedup",
            "reachable",
        ],
    )
    return df


def _plot_runtime_medians(df: pd.DataFrame, outdir: Path) -> None:
    fig, axes = plt.subplots(2, 3, figsize=(14, 8))
    axes = axes.flatten()
    for ax, family in zip(axes, FAMILIES):
        sub = df[df["graph_type"] == family].groupby("size").mean(numeric_only=True).reset_index()
        ax.plot(sub["size"], sub["dijkstra_median_ms"], marker="o", color=COLORS["dijkstra"], label="Dijkstra (median)")
        ax.plot(sub["size"], sub["dijkstra_fib_median_ms"], marker="o", color=COLORS["fib"], label="Dijkstra Fib (median)")
        ax.plot(sub["size"], sub["bmssp_median_ms"], marker="o", color=COLORS["bmssp"], label="BMSSP query (median)")
        ax.plot(sub["size"], sub["bmssp_total_with_cd_median_ms"], marker="o", color=COLORS["bmssp_total"], label="BMSSP query+CD (median)")
        ax.set_xscale("log")
        ax.set_yscale("log")
        ax.set_title(FAMILY_LABELS[family])
        ax.grid(True, which="both", linestyle="--", alpha=0.4)
        ax.set_xlabel("n")
        ax.set_ylabel("time (ms)")
    axes[-1].set_visible(False)
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="lower center", ncol=4, bbox_to_anchor=(0.5, -0.02), frameon=False)
    fig.suptitle("Median Runtime by Family")
    fig.tight_layout()
    fig.savefig(outdir / "runtime_median_family_grid.png", dpi=180, bbox_inches="tight")
    plt.close(fig)


def _plot_variability(df: pd.DataFrame, outdir: Path) -> None:
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 5))

    agg = df.groupby("size").mean(numeric_only=True).reset_index()
    x = agg["size"].to_numpy()

    ax1.plot(x, agg["dijkstra_median_ms"], marker="o", label="Dijkstra median", color=COLORS["dijkstra"])
    ax1.fill_between(x, np.maximum(0.0, agg["dijkstra_median_ms"] - agg["dijkstra_std_ms"]),
                     agg["dijkstra_median_ms"] + agg["dijkstra_std_ms"], color=COLORS["dijkstra"], alpha=0.2)
    ax1.plot(x, agg["dijkstra_fib_median_ms"], marker="o", label="Fib median", color=COLORS["fib"])
    ax1.fill_between(x, np.maximum(0.0, agg["dijkstra_fib_median_ms"] - agg["dijkstra_fib_std_ms"]),
                     agg["dijkstra_fib_median_ms"] + agg["dijkstra_fib_std_ms"], color=COLORS["fib"], alpha=0.2)
    ax1.plot(x, agg["bmssp_median_ms"], marker="o", label="BMSSP median", color=COLORS["bmssp"])
    ax1.fill_between(x, np.maximum(0.0, agg["bmssp_median_ms"] - agg["bmssp_std_ms"]),
                     agg["bmssp_median_ms"] + agg["bmssp_std_ms"], color=COLORS["bmssp"], alpha=0.2)
    ax1.set_xscale("log")
    ax1.set_yscale("log")
    ax1.grid(True, which="both", linestyle="--", alpha=0.4)
    ax1.set_title("Median +/- std across all families")
    ax1.set_xlabel("n")
    ax1.set_ylabel("time (ms)")
    ax1.legend(frameon=False)

    ax2.plot(x, agg["dijkstra_p95_ms"], marker="o", label="Dijkstra p95", color=COLORS["dijkstra"])
    ax2.plot(x, agg["dijkstra_fib_p95_ms"], marker="o", label="Fib p95", color=COLORS["fib"])
    ax2.plot(x, agg["bmssp_p95_ms"], marker="o", label="BMSSP query p95", color=COLORS["bmssp"])
    ax2.plot(x, agg["bmssp_total_with_cd_p95_ms"], marker="o", label="BMSSP query+CD p95", color=COLORS["bmssp_total"])
    ax2.set_xscale("log")
    ax2.set_yscale("log")
    ax2.grid(True, which="both", linestyle="--", alpha=0.4)
    ax2.set_title("Tail latency (p95)")
    ax2.set_xlabel("n")
    ax2.set_ylabel("time (ms)")
    ax2.legend(frameon=False)

    fig.tight_layout()
    fig.savefig(outdir / "variability_summary.png", dpi=180, bbox_inches="tight")
    plt.close(fig)


def _plot_bmssp_prep_impact(df: pd.DataFrame, outdir: Path) -> None:
    fig, ax = plt.subplots(figsize=(10, 5))
    for family in FAMILIES:
        sub = df[df["graph_type"] == family].groupby("size").mean(numeric_only=True).reset_index()
        ratio = sub["bmssp_total_with_cd_median_ms"] / sub["bmssp_median_ms"]
        ax.plot(sub["size"], ratio, marker="o", label=FAMILY_LABELS[family])
    ax.axhline(1.0, color="black", linestyle=":", linewidth=1.2)
    ax.set_xscale("log")
    ax.grid(True, which="both", linestyle="--", alpha=0.4)
    ax.set_title("Preparation-inclusive overhead: (BMSSP query+CD) / (BMSSP query)")
    ax.set_xlabel("n")
    ax.set_ylabel("ratio")
    ax.legend(frameon=False, ncol=2)
    fig.tight_layout()
    fig.savefig(outdir / "bmssp_prep_impact_ratio.png", dpi=180, bbox_inches="tight")
    plt.close(fig)


def _plot_correctness(df: pd.DataFrame, outdir: Path) -> None:
    totals = {
        "binary_vs_fib": int(df["match_count_binary_fib"].sum()),
        "binary_vs_bmssp": int(df["match_count_binary_bmssp"].sum()),
        "fib_vs_bmssp": int(df["match_count_fib_bmssp"].sum()),
    }
    trials_total = int((df["trials"] * np.ones(len(df))).sum())

    fig, ax = plt.subplots(figsize=(8, 4.5))
    names = list(totals.keys())
    vals = [totals[k] for k in names]
    bars = ax.bar(names, vals, color=["#4c78a8", "#72b7b2", "#54a24b"])
    ax.set_ylim(0, max(trials_total, max(vals)) * 1.08)
    ax.axhline(trials_total, color="black", linestyle=":", linewidth=1.2, label=f"max possible ({trials_total})")
    ax.bar_label(bars)
    ax.set_title("Distance-vector agreement counts")
    ax.set_ylabel("matching trial count")
    ax.legend(frameon=False)
    fig.tight_layout()
    fig.savefig(outdir / "correctness_match_counts.png", dpi=180, bbox_inches="tight")
    plt.close(fig)


def _plot_speedup_heatmaps(df: pd.DataFrame, outdir: Path) -> None:
    metrics = [
        ("bmssp_vs_binary_speedup", "binary/BMSSP"),
        ("bmssp_vs_fib_speedup", "fib/BMSSP"),
        ("fib_vs_binary_speedup", "binary/fib"),
    ]

    fig, axes = plt.subplots(1, 3, figsize=(15, 4.5))
    sizes = sorted(df["size"].unique())

    for ax, (col, title) in zip(axes, metrics):
        pivot = (
            df.assign(family=df["graph_type"].map(FAMILY_LABELS))
            .groupby(["family", "size"])[col]
            .median()
            .unstack(fill_value=np.nan)
            .reindex([FAMILY_LABELS[f] for f in FAMILIES])
            .reindex(columns=sizes)
        )
        im = ax.imshow(pivot.values, aspect="auto", cmap="viridis")
        ax.set_xticks(np.arange(len(sizes)))
        ax.set_xticklabels([str(s) for s in sizes], rotation=45, ha="right")
        ax.set_yticks(np.arange(len(pivot.index)))
        ax.set_yticklabels(pivot.index)
        ax.set_title(title)
        for i in range(pivot.shape[0]):
            for j in range(pivot.shape[1]):
                v = pivot.values[i, j]
                if np.isfinite(v):
                    ax.text(j, i, f"{v:.2f}", ha="center", va="center", color="white", fontsize=8)
        fig.colorbar(im, ax=ax, fraction=0.046, pad=0.04)

    fig.suptitle("Median speedup ratios by family and size")
    fig.tight_layout()
    fig.savefig(outdir / "speedup_ratio_heatmaps.png", dpi=180, bbox_inches="tight")
    plt.close(fig)


def _write_summary(df: pd.DataFrame, outdir: Path, csv_path: Path) -> None:
    per_family = (
        df.groupby("graph_type")
        .agg(
            dijkstra_median_ms=("dijkstra_median_ms", "mean"),
            dijkstra_fib_median_ms=("dijkstra_fib_median_ms", "mean"),
            bmssp_median_ms=("bmssp_median_ms", "mean"),
            bmssp_total_with_cd_median_ms=("bmssp_total_with_cd_median_ms", "mean"),
            bmssp_vs_binary_speedup=("bmssp_vs_binary_speedup", "mean"),
            bmssp_vs_fib_speedup=("bmssp_vs_fib_speedup", "mean"),
        )
        .reset_index()
    )
    per_family.to_csv(outdir / "summary_by_family.csv", index=False)

    lines = [
        "# Benchmark Methodology Summary",
        "",
        f"Input CSV: {csv_path}",
        f"Rows: {len(df)}",
        "",
        "## Benchmark controls",
        f"- trials min: {int(df['trials'].min())}",
        f"- warmup_runs min: {int(df['warmup_runs'].min())}",
        f"- randomized order flags: {sorted(df['order_randomized'].unique().tolist())}",
        "",
        "## Correctness agreement totals",
        f"- binary vs fib: {int(df['match_count_binary_fib'].sum())}",
        f"- binary vs bmssp: {int(df['match_count_binary_bmssp'].sum())}",
        f"- fib vs bmssp: {int(df['match_count_fib_bmssp'].sum())}",
        "",
        "## Generated files",
        "- runtime_median_family_grid.png",
        "- variability_summary.png",
        "- bmssp_prep_impact_ratio.png",
        "- correctness_match_counts.png",
        "- speedup_ratio_heatmaps.png",
        "- summary_by_family.csv",
    ]
    (outdir / "METHODS_AND_RESULTS.md").write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate updated figures for the benchmark profile")
    parser.add_argument("--csv", type=Path, default=Path("scale_results.csv"), help="Path to benchmark CSV")
    parser.add_argument("--outdir", type=Path, default=Path("new_figures"), help="Output directory for figures")
    args = parser.parse_args()

    outdir = args.outdir
    outdir.mkdir(parents=True, exist_ok=True)

    df = _load(args.csv)

    _plot_runtime_medians(df, outdir)
    _plot_variability(df, outdir)
    _plot_bmssp_prep_impact(df, outdir)
    _plot_correctness(df, outdir)
    _plot_speedup_heatmaps(df, outdir)
    _write_summary(df, outdir, args.csv)

    print(f"Generated figures in: {outdir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())