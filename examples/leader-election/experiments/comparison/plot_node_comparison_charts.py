#!/usr/bin/env python3
"""
Leader Election Algorithm Comparison Charts by Node Count

Generates publication-quality comparison charts for all leader election algorithms,
organized by network size (5, 10, 50, 100 nodes).

Algorithms compared:
  1. Bully
  2. Ring
  3. PraSLE (Line topology)
  4. PraSLE (Ring topology)
  5. PraSLE (Mesh topology)
  6. PraSLE (Clique topology)
  7. Adaptive-PraSLE (Clique topology)
  8. Adaptive-PraSLE (Line topology)
  9. Adaptive-PraSLE (Ring topology)
  10. Adaptive-PraSLE (Mesh topology)

Usage:
    python3 plot_node_comparison_charts.py [options]

Options:
    --results-dir, -r    Results directory (default: ../../results)
    --output, -o         Output file/directory (default: node_comparison.png)
    --metric, -m         Metric to plot: convergence, messages (default: convergence)
    --separate           Save as 4 separate files instead of combined 2x2 figure
    --dpi                DPI for output (default: 300)
    --format, -f         Output format: png, pdf, svg (default: png)

Examples:
    # Combined 2x2 convergence chart
    python3 plot_node_comparison_charts.py -m convergence -o convergence_by_nodes.png

    # Separate message overhead charts
    python3 plot_node_comparison_charts.py -m messages --separate -o ./charts/

    # PDF for thesis
    python3 plot_node_comparison_charts.py -m convergence -f pdf --dpi 600 -o thesis_chart.pdf
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
    'prasle',                    # Clique topology (default)
    'adaptive-prasle',           # Clique topology (default)
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
    'bully': '#1f77b4',           # Blue
    'ring': '#ff7f0e',            # Orange
    'prasle': '#2ca02c',          # Dark green (original PraSLE)
    'prasle-line': '#006400',     # Forest green
    'prasle-ring': '#90ee90',     # Light green
    'prasle-mesh': '#98df8a',     # Pale green
    'adaptive-prasle': '#d62728',          # Red (clique)
    'adaptive-prasle-line': '#8b0000',     # Dark red
    'adaptive-prasle-ring': '#ff6b6b',     # Light red/coral
    'adaptive-prasle-mesh': '#ffb6c1'      # Light pink
}

# Hatching patterns to distinguish topologies visually
ALGORITHM_HATCHES = {
    'bully': '',
    'ring': '',
    'prasle': '',           # Solid (clique - reference)
    'prasle-line': '\\\\',  # Reverse diagonal
    'prasle-ring': '//',    # Diagonal
    'prasle-mesh': 'xx',    # Cross-hatch
    'adaptive-prasle': '',           # Solid (clique)
    'adaptive-prasle-line': '\\\\',  # Reverse diagonal
    'adaptive-prasle-ring': '//',    # Diagonal
    'adaptive-prasle-mesh': 'xx'     # Cross-hatch
}

NODE_COUNTS = [5, 10, 50, 100]

# Publication-quality matplotlib settings
plt.rcParams.update({
    'font.size': 11,
    'axes.titlesize': 13,
    'axes.labelsize': 11,
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
    """
    Find the most recent results directory for an algorithm/node combination.

    Looks for directories matching the pattern:
      results/{algorithm}/convergence_trials/{node_count}nodes_*
    """
    # Try multiple possible path patterns
    possible_paths = [
        results_dir / algorithm / 'convergence_trials',
        results_dir / algorithm / 'convergence',
    ]

    for base_path in possible_paths:
        if not base_path.exists():
            continue

        # Find directories matching the node count pattern
        dirs = sorted(
            [d for d in base_path.iterdir()
             if d.is_dir() and f'{node_count}nodes' in d.name],
            key=lambda x: x.name,
            reverse=True  # Most recent first (timestamped directories)
        )

        if dirs:
            return dirs[0]

    return None


def load_convergence_data(results_dir: Path, algorithm: str, node_count: int) -> Optional[Dict]:
    """
    Load convergence time data for a specific algorithm and node count.

    Returns dict with: times, mean, std, median, min, max, count, ci_95
    """
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
        'median': np.median(times),
        'min': min(times),
        'max': max(times),
        'count': len(times),
        'ci_95': 1.96 * np.std(times) / np.sqrt(len(times))
    }


def load_message_data(results_dir: Path, algorithm: str, node_count: int) -> Optional[Dict]:
    """
    Load message overhead data for a specific algorithm and node count.

    Aggregates total messages from metrics.csv files across trials.
    For each trial, gets the final metrics row for each node and sums messages_sent.
    """
    latest_dir = find_latest_results_dir(results_dir, algorithm, node_count)

    if not latest_dir:
        return None

    total_messages = []

    # Look for trial directories with metrics
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
                # Group rows by node_id and keep the latest (highest timestamp) for each node
                node_final_data = {}
                for row in rows:
                    try:
                        node_id = int(row.get('node_id', 0))
                        ts = int(row.get('timestamp', 0))
                        msgs = int(row.get('messages_sent', 0))

                        # Keep only the latest timestamp for each node
                        if node_id not in node_final_data or ts > node_final_data[node_id][0]:
                            node_final_data[node_id] = (ts, msgs)
                    except (KeyError, ValueError):
                        continue

                if node_final_data:
                    # Sum messages_sent across all nodes (using their final values)
                    trial_total = sum(msgs for ts, msgs in node_final_data.values())
                    if trial_total > 0:
                        total_messages.append(trial_total)

    if not total_messages:
        return None

    return {
        'values': total_messages,
        'mean': np.mean(total_messages),
        'std': np.std(total_messages),
        'median': np.median(total_messages),
        'min': min(total_messages),
        'max': max(total_messages),
        'count': len(total_messages),
        'ci_95': 1.96 * np.std(total_messages) / np.sqrt(len(total_messages))
    }


# =============================================================================
# Plotting Functions
# =============================================================================

def plot_single_chart(ax, results_dir: Path, node_count: int,
                      metric: str = 'convergence') -> bool:
    """
    Plot a single comparison chart for a specific node count.

    Args:
        ax: Matplotlib axes object
        results_dir: Path to results directory
        node_count: Number of nodes (5, 10, 50, or 100)
        metric: 'convergence' or 'messages'

    Returns:
        bool: True if any data was plotted
    """
    bar_width = 0.08  # Narrower bars to fit 10 algorithms
    x = np.arange(1)   # Single group per chart

    has_data = False

    for i, algo in enumerate(ALGORITHMS):
        if metric == 'convergence':
            result = load_convergence_data(results_dir, algo, node_count)
            value_key = 'mean'
        else:  # messages
            result = load_message_data(results_dir, algo, node_count)
            value_key = 'mean'

        if result:
            has_data = True
            mean = result[value_key]
            std = result['std']
        else:
            mean = 0
            std = 0

        bar = ax.bar(
            x + i * bar_width,
            [mean],
            bar_width,
            label=ALGORITHM_LABELS[algo],
            color=ALGORITHM_COLORS[algo],
            hatch=ALGORITHM_HATCHES[algo],
            yerr=[std] if mean > 0 else None,
            capsize=3,
            alpha=0.85,
            edgecolor='black',
            linewidth=0.5
        )

    # Configure axes
    if metric == 'convergence':
        ax.set_ylabel('Convergence Time (ms)', fontweight='bold')
    else:
        ax.set_ylabel('Messages Sent', fontweight='bold')

    ax.set_title(f'{node_count} Nodes', fontweight='bold', fontsize=12)
    ax.set_xticks([])  # No x-axis labels (node count is in title)
    ax.set_axisbelow(True)
    ax.grid(True, axis='y', alpha=0.3)

    # Set y-axis to start from 0
    ax.set_ylim(bottom=0)

    if not has_data:
        ax.text(0.5, 0.5, 'No data available',
                transform=ax.transAxes, ha='center', va='center',
                fontsize=12, color='gray', style='italic')

    return has_data


def create_legend_handles():
    """Create legend handles for all algorithms."""
    handles = []
    for algo in ALGORITHMS:
        patch = mpatches.Patch(
            facecolor=ALGORITHM_COLORS[algo],
            edgecolor='black',
            hatch=ALGORITHM_HATCHES[algo],
            label=ALGORITHM_LABELS[algo]
        )
        handles.append(patch)
    return handles


def create_combined_figure(results_dir: Path, output_path: Path,
                          metric: str = 'convergence', dpi: int = 300,
                          format: str = 'png', same_scale: bool = False):
    """
    Create a 2x2 figure with all 4 node count charts.

    Layout:
        +-------+--------+
        | 5     | 10     |
        +-------+--------+
        | 50    | 100    |
        +-------+--------+
        |    Legend      |
        +----------------+

    Args:
        same_scale: If True, all subplots use the same y-axis scale
    """
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))

    metric_title = 'Convergence Time' if metric == 'convergence' else 'Message Overhead'
    fig.suptitle(f'Leader Election Algorithm Comparison: {metric_title}',
                 fontsize=14, fontweight='bold', y=0.98)

    axes_flat = axes.flatten()
    any_data = False

    for idx, node_count in enumerate(NODE_COUNTS):
        has_data = plot_single_chart(axes_flat[idx], results_dir, node_count, metric)
        any_data = any_data or has_data

    # Apply same y-axis scale across all subplots if requested
    if same_scale and any_data:
        # Find the maximum y-limit across all subplots
        max_ylim = max(ax.get_ylim()[1] for ax in axes_flat)
        for ax in axes_flat:
            ax.set_ylim(0, max_ylim)

    # Create unified legend at the bottom
    handles = create_legend_handles()
    fig.legend(handles=handles, loc='lower center', ncol=5,
               bbox_to_anchor=(0.5, 0.02), framealpha=0.95,
               fontsize=9)

    plt.tight_layout(rect=[0, 0.08, 1, 0.96])

    # Ensure output path has correct extension
    if not output_path.suffix:
        output_path = output_path.with_suffix(f'.{format}')

    plt.savefig(output_path, dpi=dpi, bbox_inches='tight', facecolor='white', format=format)
    plt.close()

    print(f"Combined chart saved: {output_path}")

    if not any_data:
        print("\nWarning: No data was found for any algorithm/node combination.")
        print("Make sure you have run the experiments first.")

    return any_data


def create_separate_figures(results_dir: Path, output_dir: Path,
                           metric: str = 'convergence',
                           format: str = 'png', dpi: int = 300):
    """
    Create 4 separate figures, one per node count.

    Output files: {metric}_comparison_{N}nodes.{format}
    """
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    any_data = False

    for node_count in NODE_COUNTS:
        fig, ax = plt.subplots(figsize=(12, 7))

        has_data = plot_single_chart(ax, results_dir, node_count, metric)
        any_data = any_data or has_data

        # Add legend to individual chart
        handles = create_legend_handles()
        ax.legend(handles=handles, loc='upper left', fontsize=9,
                 framealpha=0.95)

        metric_title = 'Convergence Time' if metric == 'convergence' else 'Message Overhead'
        ax.set_title(f'Leader Election {metric_title} Comparison - {node_count} Nodes',
                    fontweight='bold', fontsize=13)

        output_file = output_dir / f'{metric}_comparison_{node_count}nodes.{format}'
        plt.tight_layout()
        plt.savefig(output_file, dpi=dpi, bbox_inches='tight', facecolor='white', format=format)
        plt.close()

        status = "saved" if has_data else "saved (no data)"
        print(f"Chart {status}: {output_file}")

    if not any_data:
        print("\nWarning: No data was found for any algorithm/node combination.")
        print("Make sure you have run the experiments first.")

    return any_data


def print_data_summary(results_dir: Path, metric: str = 'convergence'):
    """Print a summary of available data."""
    print("\n" + "=" * 70)
    print(f" Data Summary ({metric})")
    print("=" * 70)
    print(f"{'Algorithm':<20} {'5':>10} {'10':>10} {'50':>10} {'100':>10}")
    print("-" * 70)

    for algo in ALGORITHMS:
        row = [ALGORITHM_LABELS[algo][:18]]
        for nc in NODE_COUNTS:
            if metric == 'convergence':
                result = load_convergence_data(results_dir, algo, nc)
            else:
                result = load_message_data(results_dir, algo, nc)

            if result:
                row.append(f"{result['mean']:.0f}")
            else:
                row.append("--")

        print(f"{row[0]:<20} {row[1]:>10} {row[2]:>10} {row[3]:>10} {row[4]:>10}")

    print("=" * 70)


# =============================================================================
# Main
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='Generate node count comparison charts for leader election algorithms',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    # Combined 2x2 convergence chart
    python3 plot_node_comparison_charts.py -m convergence -o convergence_by_nodes.png

    # Separate message overhead charts
    python3 plot_node_comparison_charts.py -m messages --separate -o ./charts/

    # PDF for thesis (high quality)
    python3 plot_node_comparison_charts.py -m convergence -f pdf --dpi 600 -o thesis_chart.pdf
        """
    )
    parser.add_argument('--results-dir', '-r', default='../../results',
                        help='Results directory (default: ../../results)')
    parser.add_argument('--output', '-o', default='node_comparison.png',
                        help='Output file/directory (default: node_comparison.png)')
    parser.add_argument('--metric', '-m', choices=['convergence', 'messages'],
                        default='convergence',
                        help='Metric to plot (default: convergence)')
    parser.add_argument('--separate', action='store_true',
                        help='Save as 4 separate files instead of combined')
    parser.add_argument('--dpi', type=int, default=300,
                        help='DPI for output (default: 300)')
    parser.add_argument('--format', '-f', choices=['png', 'pdf', 'svg'],
                        default='png',
                        help='Output format (default: png)')
    parser.add_argument('--summary', action='store_true',
                        help='Print data summary table')
    parser.add_argument('--same-scale', action='store_true',
                        help='Use same y-axis scale across all subplots for easier comparison')

    args = parser.parse_args()

    # Resolve paths
    script_dir = Path(__file__).parent
    results_dir = Path(args.results_dir)
    if not results_dir.is_absolute():
        results_dir = script_dir / results_dir
    results_dir = results_dir.resolve()

    print("=" * 70)
    print(" Leader Election Algorithm Comparison Charts")
    print("=" * 70)
    print(f"Results directory: {results_dir}")
    print(f"Metric:           {args.metric}")
    print(f"Output format:    {args.format.upper()} @ {args.dpi} DPI")
    print(f"Mode:             {'Separate files' if args.separate else 'Combined 2x2 figure'}")

    if not results_dir.exists():
        print(f"\nError: Results directory not found: {results_dir}")
        print("\nTo generate data, run experiments first:")
        print("  cd examples/leader-election/experiments/convergence")
        print("  ./run_convergence_trials.sh bully 5 100 60 4")
        print("  ./run_convergence_trials.sh ring 5 100 60 4")
        print("  # ... etc for all algorithms and node counts")
        sys.exit(1)

    # Print data summary if requested
    if args.summary:
        print_data_summary(results_dir, args.metric)

    # Generate charts
    print("\nGenerating charts...")

    if args.separate:
        output_dir = Path(args.output)
        if not output_dir.is_absolute():
            output_dir = script_dir / output_dir
        create_separate_figures(results_dir, output_dir,
                               args.metric, args.format, args.dpi)
    else:
        output_path = Path(args.output)
        if not output_path.is_absolute():
            output_path = script_dir / output_path
        create_combined_figure(results_dir, output_path,
                              args.metric, args.dpi, args.format, args.same_scale)

    print("\nDone!")


if __name__ == '__main__':
    main()
