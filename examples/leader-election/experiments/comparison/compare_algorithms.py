#!/usr/bin/env python3
"""
Cross-Algorithm Comparison Analysis for Leader Election Experiments

Generates publication-quality comparison plots across all leader election algorithms.

Plots generated:
  1. Convergence Time Comparison (bar chart with error bars)
  2. Scalability Plot (line chart: convergence time vs node count)
  3. Message Overhead Comparison (bar chart)
  4. Fault Tolerance Comparison (recovery time bar chart)
  5. State Distribution Comparison (stacked bar chart)
  6. Summary Table (CSV with all metrics)

Usage:
    python3 compare_algorithms.py [options]

Options:
    --results-dir, -r    Directory containing experiment results (default: ../../results)
    --output, -o         Output directory for plots (default: ./comparison_plots)
    --dpi                DPI for saved plots (default: 300)
    --format             Output format: png, pdf, svg (default: png)

Examples:
    python3 compare_algorithms.py
    python3 compare_algorithms.py -r ../results -o ./plots --dpi 300
    python3 compare_algorithms.py --format pdf
"""

import argparse
import csv
import os
from pathlib import Path
import sys
from typing import Dict, List, Optional, Any

try:
    import matplotlib.pyplot as plt
    import matplotlib.patches as mpatches
    import numpy as np
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("Warning: matplotlib not available. Plots will be skipped.")

# =============================================================================
# Configuration
# =============================================================================

ALGORITHMS = ['bully', 'ring', 'prasle', 'adaptive-prasle']

ALGORITHM_LABELS = {
    'bully': 'Bully',
    'ring': 'Ring',
    'prasle': 'PraSLE',
    'adaptive-prasle': 'Adaptive-PraSLE'
}

ALGORITHM_COLORS = {
    'bully': '#1f77b4',      # Blue
    'ring': '#ff7f0e',       # Orange
    'prasle': '#2ca02c',     # Green
    'adaptive-prasle': '#d62728'  # Red
}

ALGORITHM_MARKERS = {
    'bully': 'o',
    'ring': 's',
    'prasle': '^',
    'adaptive-prasle': 'D'
}

NODE_COUNTS = [5, 10, 50, 100]

# State colors for state distribution plot
STATE_COLORS = {
    'normal': '#4CAF50',     # Green
    'election': '#FF9800',   # Orange
    'waiting': '#2196F3'     # Blue
}

# Publication-quality plot settings
plt.rcParams.update({
    'font.size': 11,
    'axes.titlesize': 12,
    'axes.labelsize': 11,
    'xtick.labelsize': 10,
    'ytick.labelsize': 10,
    'legend.fontsize': 10,
    'figure.titlesize': 14,
    'figure.dpi': 100,
    'savefig.dpi': 300,
    'savefig.bbox': 'tight',
    'axes.grid': True,
    'grid.alpha': 0.3,
}) if HAS_MATPLOTLIB else None


# =============================================================================
# Data Loading Functions
# =============================================================================

def load_convergence_results(results_dir: Path, algorithm: str, node_count: int) -> Optional[Dict]:
    """Load convergence results for a specific algorithm and node count."""
    results_path = results_dir / algorithm / 'convergence_trials'

    if not results_path.exists():
        return None

    # Find the most recent results directory
    dirs = sorted(
        [d for d in results_path.iterdir() if d.is_dir() and f'{node_count}nodes' in d.name],
        key=lambda x: x.name, reverse=True
    )

    if not dirs:
        return None

    latest_dir = dirs[0]
    csv_file = latest_dir / 'convergence_times.csv'

    if not csv_file.exists():
        return None

    times = []
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                times.append(float(row['convergence_time_ms']))
            except (KeyError, ValueError):
                continue

    if not times:
        return None

    return {
        'times': times,
        'mean': np.mean(times),
        'std': np.std(times),
        'median': np.median(times),
        'min': min(times),
        'max': max(times),
        'count': len(times),
        'ci_95': 1.96 * np.std(times) / np.sqrt(len(times))  # 95% confidence interval
    }


def load_metrics_results(results_dir: Path, algorithm: str, node_count: int) -> Optional[Dict]:
    """Load detailed metrics from metrics.csv for a specific algorithm and node count."""
    results_path = results_dir / algorithm / 'convergence_trials'

    if not results_path.exists():
        return None

    # Find the most recent results directory
    dirs = sorted(
        [d for d in results_path.iterdir() if d.is_dir() and f'{node_count}nodes' in d.name],
        key=lambda x: x.name, reverse=True
    )

    if not dirs:
        return None

    latest_dir = dirs[0]

    # Aggregate metrics from all trial directories
    total_messages = []
    elections_started = []
    leader_changes = []
    time_normal = []
    time_election = []
    time_waiting = []

    for trial_dir in latest_dir.iterdir():
        if not trial_dir.is_dir() or not trial_dir.name.startswith('trial_'):
            continue

        metrics_file = trial_dir / 'metrics.csv'
        if not metrics_file.exists():
            continue

        with open(metrics_file, 'r') as f:
            reader = csv.DictReader(f)
            rows = list(reader)

            if not rows:
                continue

            # Get the last row (final metrics snapshot)
            last_row = rows[-1]

            try:
                total_messages.append(int(last_row.get('messages_sent', 0)))
                elections_started.append(int(last_row.get('elections_started', 0)))
                leader_changes.append(int(last_row.get('leader_changes', 0)))
                time_normal.append(float(last_row.get('time_in_normal_ms', 0)))
                time_election.append(float(last_row.get('time_in_election_ms', 0)))
                time_waiting.append(float(last_row.get('time_in_waiting_ms', 0)))
            except (KeyError, ValueError):
                continue

    if not total_messages:
        return None

    return {
        'messages': {
            'values': total_messages,
            'mean': np.mean(total_messages),
            'std': np.std(total_messages)
        },
        'elections': {
            'values': elections_started,
            'mean': np.mean(elections_started),
            'std': np.std(elections_started)
        },
        'leader_changes': {
            'values': leader_changes,
            'mean': np.mean(leader_changes),
            'std': np.std(leader_changes)
        },
        'state_distribution': {
            'normal': np.mean(time_normal) if time_normal else 0,
            'election': np.mean(time_election) if time_election else 0,
            'waiting': np.mean(time_waiting) if time_waiting else 0
        }
    }


def load_fault_tolerance_results(results_dir: Path, algorithm: str, node_count: int) -> Optional[Dict]:
    """Load fault tolerance results for a specific algorithm and node count."""
    results_path = results_dir / algorithm / 'fault_tolerance'

    if not results_path.exists():
        return None

    # Find the most recent results directory
    dirs = sorted(
        [d for d in results_path.iterdir() if d.is_dir() and f'{node_count}nodes' in d.name],
        key=lambda x: x.name, reverse=True
    )

    if not dirs:
        return None

    latest_dir = dirs[0]
    csv_file = latest_dir / 'crash_recovery_times.csv'

    if not csv_file.exists():
        return None

    recovery_times = []
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                recovery_times.append(float(row.get('total_recovery_time_ms', 0)))
            except (KeyError, ValueError):
                continue

    if not recovery_times:
        return None

    return {
        'times': recovery_times,
        'mean': np.mean(recovery_times),
        'std': np.std(recovery_times),
        'count': len(recovery_times)
    }


# =============================================================================
# Plotting Functions
# =============================================================================

def plot_convergence_comparison(results_dir: Path, output_dir: Path, format: str = 'png', dpi: int = 300):
    """
    Generate convergence time comparison bar chart.

    Plots mean convergence time with error bars (standard deviation) for each
    algorithm across different node counts.
    """
    print("\n=== Convergence Time Comparison ===")

    data = {}
    for algo in ALGORITHMS:
        data[algo] = {}
        for nc in NODE_COUNTS:
            result = load_convergence_results(results_dir, algo, nc)
            if result:
                data[algo][nc] = result
                print(f"  {algo:15} {nc:3} nodes: mean={result['mean']:.1f}ms, "
                      f"std={result['std']:.1f}ms, n={result['count']}")

    if not HAS_MATPLOTLIB:
        print("  Skipping plot (matplotlib not available)")
        return

    fig, ax = plt.subplots(figsize=(12, 6))

    bar_width = 0.2
    x = np.arange(len(NODE_COUNTS))

    for i, algo in enumerate(ALGORITHMS):
        if algo in data and data[algo]:
            means = [data[algo].get(nc, {}).get('mean', 0) for nc in NODE_COUNTS]
            stds = [data[algo].get(nc, {}).get('std', 0) for nc in NODE_COUNTS]

            bars = ax.bar(x + i * bar_width, means, bar_width,
                         label=ALGORITHM_LABELS[algo],
                         color=ALGORITHM_COLORS[algo],
                         yerr=stds, capsize=4, alpha=0.85)

    ax.set_xlabel('Number of Nodes', fontweight='bold')
    ax.set_ylabel('Convergence Time (ms)', fontweight='bold')
    ax.set_title('Leader Election Convergence Time Comparison', fontweight='bold', fontsize=13)
    ax.set_xticks(x + bar_width * 1.5)
    ax.set_xticklabels(NODE_COUNTS)
    ax.legend(loc='upper left', framealpha=0.9)
    ax.set_axisbelow(True)

    plt.tight_layout()
    output_file = output_dir / f'convergence_comparison.{format}'
    plt.savefig(output_file, dpi=dpi, format=format)
    plt.close()
    print(f"  Saved: {output_file}")


def plot_scalability(results_dir: Path, output_dir: Path, format: str = 'png', dpi: int = 300):
    """
    Generate scalability line plot.

    Shows how convergence time scales with the number of nodes for each algorithm.
    """
    print("\n=== Scalability Analysis ===")

    if not HAS_MATPLOTLIB:
        print("  Skipping plot (matplotlib not available)")
        return

    fig, ax = plt.subplots(figsize=(10, 6))

    has_data = False
    for algo in ALGORITHMS:
        node_counts = []
        means = []
        stds = []

        for nc in NODE_COUNTS:
            result = load_convergence_results(results_dir, algo, nc)
            if result:
                node_counts.append(nc)
                means.append(result['mean'])
                stds.append(result['std'])

        if node_counts:
            has_data = True
            ax.errorbar(node_counts, means, yerr=stds,
                       label=ALGORITHM_LABELS[algo],
                       color=ALGORITHM_COLORS[algo],
                       marker=ALGORITHM_MARKERS[algo],
                       markersize=8, linewidth=2, capsize=4)

            # Add shaded region for standard deviation
            means_arr = np.array(means)
            stds_arr = np.array(stds)
            ax.fill_between(node_counts, means_arr - stds_arr, means_arr + stds_arr,
                           color=ALGORITHM_COLORS[algo], alpha=0.15)

    if not has_data:
        print("  No data available for scalability plot")
        plt.close()
        return

    ax.set_xlabel('Number of Nodes', fontweight='bold')
    ax.set_ylabel('Convergence Time (ms)', fontweight='bold')
    ax.set_title('Algorithm Scalability: Convergence Time vs Network Size', fontweight='bold', fontsize=13)
    ax.legend(loc='upper left', framealpha=0.9)
    ax.set_xticks(NODE_COUNTS)
    ax.set_axisbelow(True)

    # Use log scale if range is large
    max_mean = max([load_convergence_results(results_dir, a, nc)['mean']
                    for a in ALGORITHMS for nc in NODE_COUNTS
                    if load_convergence_results(results_dir, a, nc)], default=0)
    min_mean = min([load_convergence_results(results_dir, a, nc)['mean']
                    for a in ALGORITHMS for nc in NODE_COUNTS
                    if load_convergence_results(results_dir, a, nc)], default=1)

    if max_mean > 0 and max_mean / max(min_mean, 1) > 100:
        ax.set_yscale('log')
        ax.set_ylabel('Convergence Time (ms) [log scale]', fontweight='bold')

    plt.tight_layout()
    output_file = output_dir / f'scalability_comparison.{format}'
    plt.savefig(output_file, dpi=dpi, format=format)
    plt.close()
    print(f"  Saved: {output_file}")


def plot_message_overhead(results_dir: Path, output_dir: Path, format: str = 'png', dpi: int = 300):
    """
    Generate message overhead comparison bar chart.

    Compares total messages sent across algorithms.
    """
    print("\n=== Message Overhead Comparison ===")

    if not HAS_MATPLOTLIB:
        print("  Skipping plot (matplotlib not available)")
        return

    data = {}
    has_data = False
    for algo in ALGORITHMS:
        data[algo] = {}
        for nc in NODE_COUNTS:
            result = load_metrics_results(results_dir, algo, nc)
            if result and 'messages' in result:
                has_data = True
                data[algo][nc] = result['messages']
                print(f"  {algo:15} {nc:3} nodes: mean={result['messages']['mean']:.1f} messages")

    if not has_data:
        print("  No message overhead data found")
        print("  (Run experiments with metrics collection enabled)")
        return

    fig, ax = plt.subplots(figsize=(12, 6))

    bar_width = 0.2
    x = np.arange(len(NODE_COUNTS))

    for i, algo in enumerate(ALGORITHMS):
        if algo in data and data[algo]:
            means = [data[algo].get(nc, {}).get('mean', 0) for nc in NODE_COUNTS]
            stds = [data[algo].get(nc, {}).get('std', 0) for nc in NODE_COUNTS]

            ax.bar(x + i * bar_width, means, bar_width,
                  label=ALGORITHM_LABELS[algo],
                  color=ALGORITHM_COLORS[algo],
                  yerr=stds, capsize=4, alpha=0.85)

    ax.set_xlabel('Number of Nodes', fontweight='bold')
    ax.set_ylabel('Total Messages Sent', fontweight='bold')
    ax.set_title('Message Overhead Comparison', fontweight='bold', fontsize=13)
    ax.set_xticks(x + bar_width * 1.5)
    ax.set_xticklabels(NODE_COUNTS)
    ax.legend(loc='upper left', framealpha=0.9)
    ax.set_axisbelow(True)

    plt.tight_layout()
    output_file = output_dir / f'message_overhead_comparison.{format}'
    plt.savefig(output_file, dpi=dpi, format=format)
    plt.close()
    print(f"  Saved: {output_file}")


def plot_fault_tolerance(results_dir: Path, output_dir: Path, format: str = 'png', dpi: int = 300):
    """
    Generate fault tolerance comparison bar chart.

    Compares recovery time after leader crash across algorithms.
    """
    print("\n=== Fault Tolerance Comparison ===")

    if not HAS_MATPLOTLIB:
        print("  Skipping plot (matplotlib not available)")
        return

    data = {}
    has_data = False
    for algo in ALGORITHMS:
        data[algo] = {}
        for nc in NODE_COUNTS:
            result = load_fault_tolerance_results(results_dir, algo, nc)
            if result:
                has_data = True
                data[algo][nc] = result
                print(f"  {algo:15} {nc:3} nodes: recovery={result['mean']:.1f}ms")

    if not has_data:
        print("  No fault tolerance data found")
        print("  (Run fault tolerance experiments first)")
        return

    fig, ax = plt.subplots(figsize=(12, 6))

    bar_width = 0.2
    x = np.arange(len(NODE_COUNTS))

    for i, algo in enumerate(ALGORITHMS):
        if algo in data and data[algo]:
            means = [data[algo].get(nc, {}).get('mean', 0) for nc in NODE_COUNTS]
            stds = [data[algo].get(nc, {}).get('std', 0) for nc in NODE_COUNTS]

            ax.bar(x + i * bar_width, means, bar_width,
                  label=ALGORITHM_LABELS[algo],
                  color=ALGORITHM_COLORS[algo],
                  yerr=stds, capsize=4, alpha=0.85)

    ax.set_xlabel('Number of Nodes', fontweight='bold')
    ax.set_ylabel('Recovery Time (ms)', fontweight='bold')
    ax.set_title('Leader Crash Recovery Time Comparison', fontweight='bold', fontsize=13)
    ax.set_xticks(x + bar_width * 1.5)
    ax.set_xticklabels(NODE_COUNTS)
    ax.legend(loc='upper left', framealpha=0.9)
    ax.set_axisbelow(True)

    plt.tight_layout()
    output_file = output_dir / f'fault_tolerance_comparison.{format}'
    plt.savefig(output_file, dpi=dpi, format=format)
    plt.close()
    print(f"  Saved: {output_file}")


def plot_state_distribution(results_dir: Path, output_dir: Path, format: str = 'png', dpi: int = 300):
    """
    Generate state distribution comparison stacked bar chart.

    Shows percentage of time spent in each state (Normal, Election, Waiting)
    for each algorithm.
    """
    print("\n=== State Distribution Comparison ===")

    if not HAS_MATPLOTLIB:
        print("  Skipping plot (matplotlib not available)")
        return

    # Use a fixed node count for state distribution comparison
    node_count = 10  # Default to 10 nodes

    data = {}
    has_data = False
    for algo in ALGORITHMS:
        result = load_metrics_results(results_dir, algo, node_count)
        if result and 'state_distribution' in result:
            has_data = True
            sd = result['state_distribution']
            total = sd['normal'] + sd['election'] + sd['waiting']
            if total > 0:
                data[algo] = {
                    'normal': 100 * sd['normal'] / total,
                    'election': 100 * sd['election'] / total,
                    'waiting': 100 * sd['waiting'] / total
                }
                print(f"  {algo:15}: Normal={data[algo]['normal']:.1f}%, "
                      f"Election={data[algo]['election']:.1f}%, "
                      f"Waiting={data[algo]['waiting']:.1f}%")

    if not has_data:
        print("  No state distribution data found")
        return

    fig, ax = plt.subplots(figsize=(10, 6))

    algos = [a for a in ALGORITHMS if a in data]
    x = np.arange(len(algos))

    normal = [data[a]['normal'] for a in algos]
    election = [data[a]['election'] for a in algos]
    waiting = [data[a]['waiting'] for a in algos]

    ax.bar(x, normal, label='Normal', color=STATE_COLORS['normal'], alpha=0.85)
    ax.bar(x, election, bottom=normal, label='Election', color=STATE_COLORS['election'], alpha=0.85)
    ax.bar(x, waiting, bottom=np.array(normal) + np.array(election),
           label='Waiting', color=STATE_COLORS['waiting'], alpha=0.85)

    ax.set_xlabel('Algorithm', fontweight='bold')
    ax.set_ylabel('Time Distribution (%)', fontweight='bold')
    ax.set_title(f'State Distribution Comparison ({node_count} nodes)', fontweight='bold', fontsize=13)
    ax.set_xticks(x)
    ax.set_xticklabels([ALGORITHM_LABELS[a] for a in algos])
    ax.legend(loc='upper right', framealpha=0.9)
    ax.set_ylim(0, 100)
    ax.set_axisbelow(True)

    plt.tight_layout()
    output_file = output_dir / f'state_distribution_comparison.{format}'
    plt.savefig(output_file, dpi=dpi, format=format)
    plt.close()
    print(f"  Saved: {output_file}")


def generate_summary_table(results_dir: Path, output_dir: Path):
    """
    Generate comprehensive summary table as CSV.

    Includes all metrics for all algorithms and node counts.
    """
    print("\n=== Generating Summary Table ===")

    summary_file = output_dir / 'summary_table.csv'

    with open(summary_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow([
            'Algorithm', 'Nodes',
            'Conv. Mean (ms)', 'Conv. Std (ms)', 'Conv. 95% CI',
            'Messages Mean', 'Messages Std',
            'Elections Mean', 'Leader Changes Mean',
            'Trials'
        ])

        for algo in ALGORITHMS:
            for nc in NODE_COUNTS:
                conv_result = load_convergence_results(results_dir, algo, nc)
                metrics_result = load_metrics_results(results_dir, algo, nc)

                if conv_result:
                    row = [
                        ALGORITHM_LABELS[algo], nc,
                        f"{conv_result['mean']:.2f}",
                        f"{conv_result['std']:.2f}",
                        f"\u00b1{conv_result['ci_95']:.2f}",
                    ]

                    if metrics_result:
                        row.extend([
                            f"{metrics_result['messages']['mean']:.1f}",
                            f"{metrics_result['messages']['std']:.1f}",
                            f"{metrics_result['elections']['mean']:.1f}",
                            f"{metrics_result['leader_changes']['mean']:.1f}",
                        ])
                    else:
                        row.extend(['N/A', 'N/A', 'N/A', 'N/A'])

                    row.append(conv_result['count'])
                    writer.writerow(row)

    print(f"  Saved: {summary_file}")

    # Also print a formatted table to console
    print("\n  Summary Table:")
    print("  " + "=" * 80)
    print(f"  {'Algorithm':<15} {'Nodes':<6} {'Conv. (ms)':<15} {'Messages':<12} {'Trials':<8}")
    print("  " + "-" * 80)

    for algo in ALGORITHMS:
        for nc in NODE_COUNTS:
            conv_result = load_convergence_results(results_dir, algo, nc)
            metrics_result = load_metrics_results(results_dir, algo, nc)

            if conv_result:
                conv_str = f"{conv_result['mean']:.1f} \u00b1 {conv_result['std']:.1f}"
                msg_str = f"{metrics_result['messages']['mean']:.0f}" if metrics_result else "N/A"
                print(f"  {ALGORITHM_LABELS[algo]:<15} {nc:<6} {conv_str:<15} {msg_str:<12} {conv_result['count']:<8}")

    print("  " + "=" * 80)


# =============================================================================
# Main
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='Compare leader election algorithms across experiments',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument('--results-dir', '-r', default='../../results',
                        help='Directory containing experiment results (default: ../../results)')
    parser.add_argument('--output', '-o', default='./comparison_plots',
                        help='Output directory for plots (default: ./comparison_plots)')
    parser.add_argument('--dpi', type=int, default=300,
                        help='DPI for saved plots (default: 300)')
    parser.add_argument('--format', '-f', choices=['png', 'pdf', 'svg'], default='png',
                        help='Output format for plots (default: png)')

    args = parser.parse_args()

    # Resolve paths relative to script directory if not absolute
    script_dir = Path(__file__).parent
    results_dir = Path(args.results_dir)
    if not results_dir.is_absolute():
        results_dir = script_dir / results_dir
    results_dir = results_dir.resolve()

    output_dir = Path(args.output)
    if not output_dir.is_absolute():
        output_dir = script_dir / output_dir
    output_dir = output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    print("=" * 70)
    print(" Leader Election Algorithm Comparison")
    print("=" * 70)
    print(f"Results directory: {results_dir.absolute()}")
    print(f"Output directory:  {output_dir.absolute()}")
    print(f"Output format:     {args.format.upper()} @ {args.dpi} DPI")

    if not results_dir.exists():
        print(f"\nError: Results directory not found: {results_dir}")
        print("Run experiments first using:")
        print("  ./experiments/run_all_experiments.sh --algorithm all")
        sys.exit(1)

    # Generate all comparison plots
    plot_convergence_comparison(results_dir, output_dir, args.format, args.dpi)
    plot_scalability(results_dir, output_dir, args.format, args.dpi)
    plot_message_overhead(results_dir, output_dir, args.format, args.dpi)
    plot_fault_tolerance(results_dir, output_dir, args.format, args.dpi)
    plot_state_distribution(results_dir, output_dir, args.format, args.dpi)
    generate_summary_table(results_dir, output_dir)

    print("\n" + "=" * 70)
    print(" Comparison Complete")
    print("=" * 70)
    print(f"\nAll outputs saved to: {output_dir.absolute()}")
    print("\nGenerated files:")
    print(f"  - convergence_comparison.{args.format}")
    print(f"  - scalability_comparison.{args.format}")
    print(f"  - message_overhead_comparison.{args.format}")
    print(f"  - fault_tolerance_comparison.{args.format}")
    print(f"  - state_distribution_comparison.{args.format}")
    print(f"  - summary_table.csv")


if __name__ == '__main__':
    main()
