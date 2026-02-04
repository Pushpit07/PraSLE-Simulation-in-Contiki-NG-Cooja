#!/usr/bin/env python3
"""
Messages vs Convergence Time Chart for Leader Election Algorithms

Generates a scatter plot showing the relationship between message overhead
and convergence time for all algorithms across different network sizes.

This visualization helps identify the trade-off between communication cost
and election speed for each algorithm.

Usage:
    python3 plot_messages_vs_convergence.py [options]

Options:
    --results-dir, -r    Results directory (default: ../../results)
    --output, -o         Output file (default: ./messages_vs_convergence.png)
    --dpi                DPI for output (default: 300)
    --format, -f         Output format: png, pdf, svg (default: png)

Examples:
    python3 plot_messages_vs_convergence.py
    python3 plot_messages_vs_convergence.py -f pdf --dpi 600 -o thesis_tradeoff.pdf
"""

import argparse
import csv
from pathlib import Path
import sys
from typing import Dict, List, Optional, Tuple

try:
    import matplotlib.pyplot as plt
    import matplotlib.patches as mpatches
    import numpy as np
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("Error: matplotlib and numpy are required.")
    print("Install with: pip install matplotlib numpy")
    sys.exit(1)

# =============================================================================
# Configuration
# =============================================================================

ALGORITHMS = [
    'bully',
    'ring',
    'prasle-line',
    'prasle-ring',
    'prasle-mesh',
    'prasle',
    'adaptive-prasle',
    'adaptive-prasle-line',
    'adaptive-prasle-ring',
    'adaptive-prasle-mesh'
]

ALGORITHM_LABELS = {
    'bully': 'Bully',
    'ring': 'Ring',
    'prasle': 'PraSLE (Clique)',
    'prasle-line': 'PraSLE (Line)',
    'prasle-ring': 'PraSLE (Ring)',
    'prasle-mesh': 'PraSLE (Mesh)',
    'adaptive-prasle': 'Adaptive (Clique)',
    'adaptive-prasle-line': 'Adaptive (Line)',
    'adaptive-prasle-ring': 'Adaptive (Ring)',
    'adaptive-prasle-mesh': 'Adaptive (Mesh)'
}

ALGORITHM_COLORS = {
    'bully': '#1f77b4',
    'ring': '#ff7f0e',
    'prasle': '#2ca02c',
    'prasle-line': '#006400',
    'prasle-ring': '#32CD32',
    'prasle-mesh': '#98df8a',
    'adaptive-prasle': '#d62728',
    'adaptive-prasle-line': '#8b0000',
    'adaptive-prasle-ring': '#ff6b6b',
    'adaptive-prasle-mesh': '#ffb6c1'
}

ALGORITHM_MARKERS = {
    'bully': 'o',
    'ring': 's',
    'prasle': '^',
    'prasle-line': '<',
    'prasle-ring': '>',
    'prasle-mesh': 'v',
    'adaptive-prasle': 'D',
    'adaptive-prasle-line': 'p',
    'adaptive-prasle-ring': 'h',
    'adaptive-prasle-mesh': 'H'
}

NODE_COUNTS = [5, 10, 50, 100]

NODE_SIZES = {
    5: 100,
    10: 150,
    50: 200,
    100: 250
}

# Publication-quality matplotlib settings
plt.rcParams.update({
    'font.size': 11,
    'axes.titlesize': 13,
    'axes.labelsize': 12,
    'xtick.labelsize': 10,
    'ytick.labelsize': 10,
    'legend.fontsize': 9,
    'figure.titlesize': 14,
    'figure.dpi': 100,
    'savefig.dpi': 300,
    'savefig.bbox': 'tight',
    'axes.grid': True,
    'grid.alpha': 0.3,
})


# =============================================================================
# Data Loading Functions
# =============================================================================

def find_latest_results_dir(results_dir: Path, algorithm: str, node_count: int) -> Optional[Path]:
    """Find the most recent results directory for an algorithm/node combination."""
    possible_paths = [
        results_dir / algorithm / 'convergence_trials',
        results_dir / algorithm / 'convergence',
    ]

    for base_path in possible_paths:
        if not base_path.exists():
            continue

        dirs = sorted(
            [d for d in base_path.iterdir()
             if d.is_dir() and f'{node_count}nodes' in d.name],
            key=lambda x: x.name,
            reverse=True
        )

        if dirs:
            return dirs[0]

    return None


def load_convergence_data(results_dir: Path, algorithm: str, node_count: int) -> Optional[Dict]:
    """Load convergence time data for a specific algorithm and node count."""
    latest_dir = find_latest_results_dir(results_dir, algorithm, node_count)

    if not latest_dir:
        return None

    csv_file = latest_dir / 'convergence_times.csv'
    if not csv_file.exists():
        return None

    times = []
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                time_val = float(row.get('convergence_time_ms', 0))
                if time_val > 0:
                    times.append(time_val)
            except (KeyError, ValueError):
                continue

    if not times:
        return None

    return {
        'times': times,
        'mean': np.mean(times),
        'std': np.std(times),
        'count': len(times),
        'ci_95': 1.96 * np.std(times) / np.sqrt(len(times))
    }


def load_message_data(results_dir: Path, algorithm: str, node_count: int) -> Optional[Dict]:
    """Load message overhead data for a specific algorithm and node count."""
    latest_dir = find_latest_results_dir(results_dir, algorithm, node_count)

    if not latest_dir:
        return None

    total_messages = []

    for trial_dir in latest_dir.iterdir():
        if not trial_dir.is_dir():
            continue

        metrics_file = trial_dir / 'metrics.csv'
        if not metrics_file.exists():
            continue

        with open(metrics_file, 'r') as f:
            reader = csv.DictReader(f)
            rows = list(reader)

            if rows:
                node_final_data = {}
                for row in rows:
                    try:
                        node_id = int(row.get('node_id', 0))
                        ts = int(row.get('timestamp', 0))
                        msgs = int(row.get('messages_sent', 0))

                        if node_id not in node_final_data or ts > node_final_data[node_id][0]:
                            node_final_data[node_id] = (ts, msgs)
                    except (KeyError, ValueError):
                        continue

                if node_final_data:
                    trial_total = sum(msgs for ts, msgs in node_final_data.values())
                    if trial_total > 0:
                        total_messages.append(trial_total)

    if not total_messages:
        return None

    return {
        'values': total_messages,
        'mean': np.mean(total_messages),
        'std': np.std(total_messages),
        'count': len(total_messages),
        'ci_95': 1.96 * np.std(total_messages) / np.sqrt(len(total_messages))
    }


# =============================================================================
# Plotting Functions
# =============================================================================

def plot_messages_vs_convergence(results_dir: Path, output_path: Path,
                                  format: str = 'png', dpi: int = 300) -> Optional[Path]:
    """
    Generate a scatter plot of messages vs convergence time.

    Each algorithm is shown with a different color and marker.
    Point size indicates network size (larger = more nodes).
    Error bars show standard deviation.
    """
    fig, ax = plt.subplots(figsize=(12, 8))

    has_data = False
    legend_handles = []

    # Collect all data points
    all_data = []

    for algo in ALGORITHMS:
        for nc in NODE_COUNTS:
            conv = load_convergence_data(results_dir, algo, nc)
            msg = load_message_data(results_dir, algo, nc)

            if conv and msg:
                all_data.append({
                    'algorithm': algo,
                    'nodes': nc,
                    'conv_mean': conv['mean'],
                    'conv_std': conv['std'],
                    'msg_mean': msg['mean'],
                    'msg_std': msg['std']
                })
                has_data = True

    if not has_data:
        print("No data found for messages vs convergence plot")
        plt.close()
        return None

    # Plot each algorithm
    plotted_algos = set()

    for data in all_data:
        algo = data['algorithm']
        nc = data['nodes']

        # Plot point with error bars
        ax.errorbar(
            data['msg_mean'], data['conv_mean'],
            xerr=data['msg_std'], yerr=data['conv_std'],
            fmt=ALGORITHM_MARKERS[algo],
            color=ALGORITHM_COLORS[algo],
            markersize=NODE_SIZES[nc] ** 0.5,  # Scale marker size
            alpha=0.8,
            capsize=3,
            capthick=1,
            elinewidth=1,
            label=ALGORITHM_LABELS[algo] if algo not in plotted_algos else None
        )

        # Add node count annotation
        ax.annotate(
            f'{nc}',
            (data['msg_mean'], data['conv_mean']),
            textcoords="offset points",
            xytext=(5, 5),
            fontsize=8,
            alpha=0.7
        )

        plotted_algos.add(algo)

    # Configure axes
    ax.set_xlabel('Total Messages Sent', fontweight='bold', fontsize=12)
    ax.set_ylabel('Convergence Time (ms)', fontweight='bold', fontsize=12)
    ax.set_title('Message Overhead vs Convergence Time Trade-off\n(Numbers indicate node count)',
                 fontweight='bold', fontsize=13)

    # Use log scale if range is large
    msg_values = [d['msg_mean'] for d in all_data]
    conv_values = [d['conv_mean'] for d in all_data]

    if max(msg_values) / max(min(msg_values), 1) > 50:
        ax.set_xscale('log')
        ax.set_xlabel('Total Messages Sent (log scale)', fontweight='bold', fontsize=12)

    ax.set_ylim(bottom=0)
    ax.grid(True, alpha=0.3)
    ax.set_axisbelow(True)

    # Create legend
    ax.legend(loc='upper left', framealpha=0.95, ncol=2)

    # Add annotation for ideal region
    ax.annotate(
        'Ideal\n(low messages,\nfast convergence)',
        xy=(0.05, 0.05),
        xycoords='axes fraction',
        fontsize=10,
        fontstyle='italic',
        alpha=0.6,
        bbox=dict(boxstyle='round,pad=0.3', facecolor='lightgreen', alpha=0.3)
    )

    plt.tight_layout()

    # Save figure
    if not output_path.suffix:
        output_path = output_path.with_suffix(f'.{format}')

    plt.savefig(output_path, dpi=dpi, bbox_inches='tight', facecolor='white', format=format)
    plt.close()

    print(f"Saved: {output_path}")
    return output_path


def plot_messages_vs_convergence_by_nodes(results_dir: Path, output_path: Path,
                                           format: str = 'png', dpi: int = 300) -> Optional[Path]:
    """
    Generate a 2x2 grid of scatter plots, one for each node count.

    This provides a clearer view of the trade-off at each network size.
    """
    fig, axes = plt.subplots(2, 2, figsize=(14, 12))
    axes_flat = axes.flatten()

    has_any_data = False

    for idx, nc in enumerate(NODE_COUNTS):
        ax = axes_flat[idx]
        has_data = False

        for algo in ALGORITHMS:
            conv = load_convergence_data(results_dir, algo, nc)
            msg = load_message_data(results_dir, algo, nc)

            if conv and msg:
                has_data = True
                has_any_data = True

                ax.errorbar(
                    msg['mean'], conv['mean'],
                    xerr=msg['std'], yerr=conv['std'],
                    fmt=ALGORITHM_MARKERS[algo],
                    color=ALGORITHM_COLORS[algo],
                    markersize=12,
                    alpha=0.8,
                    capsize=4,
                    capthick=1.5,
                    elinewidth=1.5,
                    label=ALGORITHM_LABELS[algo]
                )

        ax.set_xlabel('Messages Sent', fontweight='bold')
        ax.set_ylabel('Convergence Time (ms)', fontweight='bold')
        ax.set_title(f'{nc} Nodes', fontweight='bold', fontsize=12)
        ax.set_ylim(bottom=0)
        ax.grid(True, alpha=0.3)
        ax.set_axisbelow(True)

        if not has_data:
            ax.text(0.5, 0.5, 'No data available',
                   transform=ax.transAxes, ha='center', va='center',
                   fontsize=12, color='gray', style='italic')

    if not has_any_data:
        print("No data found for messages vs convergence plot")
        plt.close()
        return None

    # Create unified legend
    handles, labels = axes_flat[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc='lower center', ncol=5,
               bbox_to_anchor=(0.5, 0.02), framealpha=0.95, fontsize=9)

    fig.suptitle('Message Overhead vs Convergence Time Trade-off',
                 fontsize=14, fontweight='bold', y=0.98)

    plt.tight_layout(rect=[0, 0.08, 1, 0.96])

    # Save figure
    output_path_grid = output_path.parent / f"{output_path.stem}_by_nodes{output_path.suffix}"
    if not output_path_grid.suffix:
        output_path_grid = output_path_grid.with_suffix(f'.{format}')

    plt.savefig(output_path_grid, dpi=dpi, bbox_inches='tight', facecolor='white', format=format)
    plt.close()

    print(f"Saved: {output_path_grid}")
    return output_path_grid


def print_tradeoff_summary(results_dir: Path):
    """Print a summary table of messages vs convergence for all algorithms."""
    print("\n" + "=" * 90)
    print(" Messages vs Convergence Time Summary")
    print("=" * 90)
    print(f"{'Algorithm':<22} {'Nodes':<8} {'Conv. (ms)':<18} {'Messages':<18} {'Ratio':<12}")
    print("-" * 90)

    for algo in ALGORITHMS:
        for nc in NODE_COUNTS:
            conv = load_convergence_data(results_dir, algo, nc)
            msg = load_message_data(results_dir, algo, nc)

            if conv and msg:
                conv_str = f"{conv['mean']:.0f} +/- {conv['std']:.0f}"
                msg_str = f"{msg['mean']:.0f} +/- {msg['std']:.0f}"
                # Ratio: messages per ms of convergence time
                ratio = msg['mean'] / conv['mean'] if conv['mean'] > 0 else 0
                ratio_str = f"{ratio:.2f} msg/ms"
                print(f"{ALGORITHM_LABELS[algo]:<22} {nc:<8} {conv_str:<18} {msg_str:<18} {ratio_str:<12}")

    print("=" * 90)


# =============================================================================
# Main
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='Generate messages vs convergence time scatter plot',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    python3 plot_messages_vs_convergence.py
    python3 plot_messages_vs_convergence.py -f pdf --dpi 600 -o thesis_tradeoff.pdf
        """
    )
    parser.add_argument('--results-dir', '-r', default='../../results',
                        help='Results directory (default: ../../results)')
    parser.add_argument('--output', '-o', default='../../results/comparison_charts/messages_vs_convergence.png',
                        help='Output file (default: ../../results/comparison_charts/messages_vs_convergence.png)')
    parser.add_argument('--dpi', type=int, default=300,
                        help='DPI for output (default: 300)')
    parser.add_argument('--format', '-f', choices=['png', 'pdf', 'svg'],
                        default='png',
                        help='Output format (default: png)')
    parser.add_argument('--summary', action='store_true',
                        help='Print data summary table')

    args = parser.parse_args()

    # Resolve paths
    script_dir = Path(__file__).parent
    results_dir = Path(args.results_dir)
    if not results_dir.is_absolute():
        results_dir = script_dir / results_dir
    results_dir = results_dir.resolve()

    output_path = Path(args.output)
    if not output_path.is_absolute():
        output_path = script_dir / output_path
    output_path = output_path.resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)

    print("=" * 70)
    print(" Messages vs Convergence Time Chart")
    print("=" * 70)
    print(f"Results directory: {results_dir}")
    print(f"Output file:       {output_path}")
    print(f"Output format:     {args.format.upper()} @ {args.dpi} DPI")

    if not results_dir.exists():
        print(f"\nError: Results directory not found: {results_dir}")
        sys.exit(1)

    # Print summary if requested
    if args.summary:
        print_tradeoff_summary(results_dir)

    # Generate charts
    print("\nGenerating charts...")

    # Main scatter plot (all data points)
    result1 = plot_messages_vs_convergence(results_dir, output_path, args.format, args.dpi)

    # Grid plot (by node count)
    result2 = plot_messages_vs_convergence_by_nodes(results_dir, output_path, args.format, args.dpi)

    if result1 or result2:
        print("\nDone!")
    else:
        print("\nNo charts generated (no data available).")


if __name__ == '__main__':
    main()
