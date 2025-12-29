#!/usr/bin/env python3
"""
Cross-Algorithm Comparison Analysis

Compares metrics across different leader election algorithms.

Usage:
    python3 compare_algorithms.py --results-dir ../results --output comparison/
"""

import argparse
import csv
import os
from pathlib import Path
import sys

try:
    import matplotlib.pyplot as plt
    import numpy as np
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("Warning: matplotlib not available. Plots will be skipped.")

ALGORITHMS = ['bully', 'ring', 'prasle', 'adaptive-prasle']
ALGORITHM_LABELS = {
    'bully': 'Bully',
    'ring': 'Ring',
    'prasle': 'PraSLE',
    'adaptive-prasle': 'PraSLE-Custom'
}
ALGORITHM_COLORS = {
    'bully': '#1f77b4',
    'ring': '#ff7f0e',
    'prasle': '#2ca02c',
    'adaptive-prasle': '#d62728'
}

NODE_COUNTS = [5, 10, 50, 100]


def load_convergence_results(results_dir, algorithm, node_count):
    """Load convergence results for a specific algorithm and node count."""
    results_path = Path(results_dir) / algorithm / 'convergence_trials'

    if not results_path.exists():
        return None

    # Find the most recent results directory
    dirs = sorted([d for d in results_path.iterdir() if d.is_dir() and f'{node_count}nodes' in d.name],
                  key=lambda x: x.name, reverse=True)

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
        'mean': np.mean(times) if HAS_MATPLOTLIB else sum(times) / len(times),
        'std': np.std(times) if HAS_MATPLOTLIB else 0,
        'median': np.median(times) if HAS_MATPLOTLIB else sorted(times)[len(times)//2],
        'min': min(times),
        'max': max(times),
        'count': len(times)
    }


def compare_convergence_time(results_dir, output_dir):
    """Compare convergence time across algorithms."""
    print("\n=== Convergence Time Comparison ===")

    data = {}
    for algo in ALGORITHMS:
        data[algo] = {}
        for nc in NODE_COUNTS:
            result = load_convergence_results(results_dir, algo, nc)
            if result:
                data[algo][nc] = result
                print(f"{algo:15} {nc:3} nodes: mean={result['mean']:.2f}ms, "
                      f"std={result['std']:.2f}ms, n={result['count']}")

    if not HAS_MATPLOTLIB:
        print("\nSkipping plot generation (matplotlib not available)")
        return

    # Create comparison plot
    fig, ax = plt.subplots(figsize=(10, 6))

    bar_width = 0.2
    x = np.arange(len(NODE_COUNTS))

    for i, algo in enumerate(ALGORITHMS):
        if algo in data and data[algo]:
            means = [data[algo].get(nc, {}).get('mean', 0) for nc in NODE_COUNTS]
            stds = [data[algo].get(nc, {}).get('std', 0) for nc in NODE_COUNTS]

            ax.bar(x + i * bar_width, means, bar_width,
                   label=ALGORITHM_LABELS[algo],
                   color=ALGORITHM_COLORS[algo],
                   yerr=stds, capsize=3)

    ax.set_xlabel('Number of Nodes')
    ax.set_ylabel('Convergence Time (ms)')
    ax.set_title('Convergence Time Comparison Across Algorithms')
    ax.set_xticks(x + bar_width * 1.5)
    ax.set_xticklabels(NODE_COUNTS)
    ax.legend()
    ax.grid(True, alpha=0.3, axis='y')

    plt.tight_layout()
    output_file = output_dir / 'convergence_comparison.png'
    plt.savefig(output_file, dpi=300)
    plt.close()
    print(f"\nPlot saved: {output_file}")


def compare_message_overhead(results_dir, output_dir):
    """Compare message overhead across algorithms."""
    print("\n=== Message Overhead Comparison ===")

    # This would analyze metrics.csv files for message counts
    # For now, just a placeholder
    print("Message overhead comparison not yet implemented")
    print("(Requires running full experiments with metrics collection)")


def generate_summary_table(results_dir, output_dir):
    """Generate a summary table of all results."""
    print("\n=== Summary Table ===")

    summary_file = output_dir / 'summary.csv'

    with open(summary_file, 'w') as f:
        writer = csv.writer(f)
        writer.writerow(['Algorithm', 'Nodes', 'Mean (ms)', 'Std (ms)', 'Median (ms)', 'Min (ms)', 'Max (ms)', 'Trials'])

        for algo in ALGORITHMS:
            for nc in NODE_COUNTS:
                result = load_convergence_results(results_dir, algo, nc)
                if result:
                    writer.writerow([
                        algo, nc,
                        f"{result['mean']:.2f}",
                        f"{result['std']:.2f}",
                        f"{result['median']:.2f}",
                        f"{result['min']:.2f}",
                        f"{result['max']:.2f}",
                        result['count']
                    ])

    print(f"Summary saved: {summary_file}")


def main():
    parser = argparse.ArgumentParser(description='Compare leader election algorithms')
    parser.add_argument('--results-dir', '-r', default='../results',
                        help='Directory containing experiment results')
    parser.add_argument('--output', '-o', default='.',
                        help='Output directory for plots and summaries')

    args = parser.parse_args()

    results_dir = Path(args.results_dir)
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    if not results_dir.exists():
        print(f"Error: Results directory not found: {results_dir}")
        print("Run experiments first using run_all_experiments.sh")
        sys.exit(1)

    print(f"Results directory: {results_dir.absolute()}")
    print(f"Output directory: {output_dir.absolute()}")

    # Run comparisons
    compare_convergence_time(results_dir, output_dir)
    compare_message_overhead(results_dir, output_dir)
    generate_summary_table(results_dir, output_dir)

    print("\n=== Done ===")


if __name__ == '__main__':
    main()
