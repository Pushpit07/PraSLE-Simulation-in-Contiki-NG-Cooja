#!/usr/bin/env python3
"""
Leader Election Algorithm Network Partition Comparison Charts by Node Count

Generates publication-quality comparison charts for network partition behavior
across all leader election algorithms, organized by network size (5, 10, 50, 100 nodes).

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
    python3 plot_partition_comparison_charts.py [options]

Options:
    --results-dir, -r    Results directory (default: ../../results)
    --output, -o         Output file/directory (default: partition_comparison.png)
    --partition, -p      Partition to show: a, b, both (default: both)
    --separate           Save as 4 separate files instead of combined 2x2 figure
    --dpi                DPI for output (default: 300)
    --format, -f         Output format: png, pdf, svg (default: png)

Examples:
    # Combined 2x2 partition chart (both partitions)
    python3 plot_partition_comparison_charts.py -o partition_by_nodes.png

    # Only partition A
    python3 plot_partition_comparison_charts.py -p a -o partition_a_by_nodes.png

    # Separate charts
    python3 plot_partition_comparison_charts.py --separate -o ./charts/
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

ALGORITHM_HATCHES = {
    'bully': '',
    'ring': '',
    'prasle': '',
    'prasle-line': '\\\\',
    'prasle-ring': '//',
    'prasle-mesh': 'xx',
    'adaptive-prasle': '',
    'adaptive-prasle-line': '\\\\',
    'adaptive-prasle-ring': '//',
    'adaptive-prasle-mesh': 'xx'
}

NODE_COUNTS = [5, 10, 50, 100]

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
    Find the most recent network partition results directory.
    """
    possible_paths = [
        results_dir / algorithm / 'network_partition',
        results_dir / algorithm / 'partition',
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


def load_partition_data(results_dir: Path, algorithm: str, node_count: int) -> Optional[Dict]:
    """
    Load network partition data for a specific algorithm and node count.

    Returns dict with partition_a and partition_b statistics.
    """
    latest_dir = find_latest_results_dir(results_dir, algorithm, node_count)

    if not latest_dir:
        return None

    csv_file = latest_dir / 'partition_times.csv'
    if not csv_file.exists():
        return None

    partition_a_times = []
    partition_b_times = []
    split_brain_count = 0
    total_trials = 0

    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            total_trials += 1
            try:
                # Partition A convergence time
                if 'partition_a_convergence_ms' in row and row['partition_a_convergence_ms'] != 'NA':
                    val = float(row['partition_a_convergence_ms'])
                    if val > 0:
                        partition_a_times.append(val)

                # Partition B convergence time
                if 'partition_b_convergence_ms' in row and row['partition_b_convergence_ms'] != 'NA':
                    val = float(row['partition_b_convergence_ms'])
                    if val > 0:
                        partition_b_times.append(val)

                # Split-brain detection
                if row.get('split_brain', '').lower() == 'true':
                    split_brain_count += 1

            except (KeyError, ValueError):
                continue

    if not partition_a_times and not partition_b_times:
        return None

    result = {
        'total_trials': total_trials,
        'split_brain_count': split_brain_count,
        'split_brain_pct': (split_brain_count / total_trials * 100) if total_trials > 0 else 0
    }

    if partition_a_times:
        result['partition_a'] = {
            'times': partition_a_times,
            'mean': np.mean(partition_a_times),
            'std': np.std(partition_a_times),
            'median': np.median(partition_a_times),
            'min': min(partition_a_times),
            'max': max(partition_a_times),
            'count': len(partition_a_times),
            'ci_95': 1.96 * np.std(partition_a_times) / np.sqrt(len(partition_a_times))
        }

    if partition_b_times:
        result['partition_b'] = {
            'times': partition_b_times,
            'mean': np.mean(partition_b_times),
            'std': np.std(partition_b_times),
            'median': np.median(partition_b_times),
            'min': min(partition_b_times),
            'max': max(partition_b_times),
            'count': len(partition_b_times),
            'ci_95': 1.96 * np.std(partition_b_times) / np.sqrt(len(partition_b_times))
        }

    return result


# =============================================================================
# Plotting Functions
# =============================================================================

def plot_single_chart(ax, results_dir: Path, node_count: int,
                      partition: str = 'both') -> bool:
    """
    Plot a single comparison chart for a specific node count.

    partition: 'a', 'b', or 'both'
    """
    has_data = False

    if partition == 'both':
        # Show both partitions side by side for each algorithm
        bar_width = 0.04  # Narrower to fit 20 bars (10 algorithms x 2 partitions)
        x = np.arange(1)

        for i, algo in enumerate(ALGORITHMS):
            result = load_partition_data(results_dir, algo, node_count)

            # Partition A
            if result and 'partition_a' in result:
                has_data = True
                mean_a = result['partition_a']['mean']
                std_a = result['partition_a']['std']
            else:
                mean_a = 0
                std_a = 0

            # Partition B
            if result and 'partition_b' in result:
                has_data = True
                mean_b = result['partition_b']['mean']
                std_b = result['partition_b']['std']
            else:
                mean_b = 0
                std_b = 0

            # Draw partition A bar (lighter shade)
            ax.bar(
                x + (i * 2) * bar_width,
                [mean_a],
                bar_width,
                color=ALGORITHM_COLORS[algo],
                hatch=ALGORITHM_HATCHES[algo],
                yerr=[std_a] if mean_a > 0 else None,
                capsize=2,
                alpha=0.6,
                edgecolor='black',
                linewidth=0.5
            )

            # Draw partition B bar (darker shade)
            ax.bar(
                x + (i * 2 + 1) * bar_width,
                [mean_b],
                bar_width,
                color=ALGORITHM_COLORS[algo],
                hatch=ALGORITHM_HATCHES[algo],
                yerr=[std_b] if mean_b > 0 else None,
                capsize=2,
                alpha=0.95,
                edgecolor='black',
                linewidth=0.5
            )
    else:
        # Show single partition
        bar_width = 0.08  # Narrower bars to fit 10 algorithms
        x = np.arange(1)
        partition_key = f'partition_{partition}'

        for i, algo in enumerate(ALGORITHMS):
            result = load_partition_data(results_dir, algo, node_count)

            if result and partition_key in result:
                has_data = True
                mean = result[partition_key]['mean']
                std = result[partition_key]['std']
            else:
                mean = 0
                std = 0

            ax.bar(
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

    ax.set_ylabel('Convergence Time (ms)', fontweight='bold')
    ax.set_title(f'{node_count} Nodes', fontweight='bold', fontsize=12)
    ax.set_xticks([])
    ax.set_axisbelow(True)
    ax.grid(True, axis='y', alpha=0.3)
    ax.set_ylim(bottom=0)

    if not has_data:
        ax.text(0.5, 0.5, 'No data available',
                transform=ax.transAxes, ha='center', va='center',
                fontsize=12, color='gray', style='italic')

    return has_data


def create_legend_handles(partition: str = 'both'):
    """Create legend handles for all algorithms."""
    handles = []

    if partition == 'both':
        # Create handles showing both partitions
        for algo in ALGORITHMS:
            # Partition A (lighter)
            patch_a = mpatches.Patch(
                facecolor=ALGORITHM_COLORS[algo],
                edgecolor='black',
                hatch=ALGORITHM_HATCHES[algo],
                alpha=0.6,
                label=f'{ALGORITHM_LABELS[algo]} (A)'
            )
            handles.append(patch_a)
    else:
        for algo in ALGORITHMS:
            patch = mpatches.Patch(
                facecolor=ALGORITHM_COLORS[algo],
                edgecolor='black',
                hatch=ALGORITHM_HATCHES[algo],
                label=ALGORITHM_LABELS[algo]
            )
            handles.append(patch)

    return handles


def create_simple_legend_handles():
    """Create simplified legend handles (algorithm only, plus partition indicator)."""
    handles = []
    for algo in ALGORITHMS:
        patch = mpatches.Patch(
            facecolor=ALGORITHM_COLORS[algo],
            edgecolor='black',
            hatch=ALGORITHM_HATCHES[algo],
            label=ALGORITHM_LABELS[algo]
        )
        handles.append(patch)

    # Add partition indicators
    handles.append(mpatches.Patch(facecolor='gray', alpha=0.6, label='Partition A (light)'))
    handles.append(mpatches.Patch(facecolor='gray', alpha=0.95, label='Partition B (dark)'))

    return handles


def create_combined_figure(results_dir: Path, output_path: Path, partition: str = 'both',
                          dpi: int = 300, format: str = 'png', same_scale: bool = False):
    """Create a 2x2 figure with all 4 node count charts.

    Args:
        same_scale: If True, all subplots use the same y-axis scale
    """
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))

    if partition == 'both':
        title = 'Network Partition Convergence Time (Both Partitions)'
    else:
        title = f'Network Partition Convergence Time (Partition {partition.upper()})'

    fig.suptitle(title, fontsize=14, fontweight='bold', y=0.98)

    axes_flat = axes.flatten()
    any_data = False

    for idx, node_count in enumerate(NODE_COUNTS):
        has_data = plot_single_chart(axes_flat[idx], results_dir, node_count, partition)
        any_data = any_data or has_data

    # Apply same y-axis scale across all subplots if requested
    if same_scale and any_data:
        max_ylim = max(ax.get_ylim()[1] for ax in axes_flat)
        for ax in axes_flat:
            ax.set_ylim(0, max_ylim)

    if partition == 'both':
        handles = create_simple_legend_handles()
        fig.legend(handles=handles, loc='lower center', ncol=6,
                   bbox_to_anchor=(0.5, 0.02), framealpha=0.95, fontsize=8)
    else:
        handles = create_legend_handles(partition)
        fig.legend(handles=handles, loc='lower center', ncol=5,
                   bbox_to_anchor=(0.5, 0.02), framealpha=0.95, fontsize=9)

    plt.tight_layout(rect=[0, 0.08, 1, 0.96])

    if not output_path.suffix:
        output_path = output_path.with_suffix(f'.{format}')

    plt.savefig(output_path, dpi=dpi, bbox_inches='tight', facecolor='white', format=format)
    plt.close()

    print(f"Combined chart saved: {output_path}")

    if not any_data:
        print("\nWarning: No data was found for any algorithm/node combination.")
        print("Make sure you have run the network partition experiments first.")

    return any_data


def create_separate_figures(results_dir: Path, output_dir: Path, partition: str = 'both',
                           format: str = 'png', dpi: int = 300):
    """Create 4 separate figures, one per node count."""
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    any_data = False

    for node_count in NODE_COUNTS:
        fig, ax = plt.subplots(figsize=(12, 7))

        has_data = plot_single_chart(ax, results_dir, node_count, partition)
        any_data = any_data or has_data

        if partition == 'both':
            handles = create_simple_legend_handles()
            ax.legend(handles=handles, loc='upper left', fontsize=8, framealpha=0.95)
            title = f'Network Partition Convergence (Both) - {node_count} Nodes'
        else:
            handles = create_legend_handles(partition)
            ax.legend(handles=handles, loc='upper left', fontsize=9, framealpha=0.95)
            title = f'Network Partition Convergence (Partition {partition.upper()}) - {node_count} Nodes'

        ax.set_title(title, fontweight='bold', fontsize=13)

        suffix = f'_{partition}' if partition != 'both' else '_both'
        output_file = output_dir / f'partition{suffix}_comparison_{node_count}nodes.{format}'
        plt.tight_layout()
        plt.savefig(output_file, dpi=dpi, bbox_inches='tight', facecolor='white', format=format)
        plt.close()

        status = "saved" if has_data else "saved (no data)"
        print(f"Chart {status}: {output_file}")

    if not any_data:
        print("\nWarning: No data was found for any algorithm/node combination.")

    return any_data


def print_data_summary(results_dir: Path):
    """Print a summary of available data."""
    print("\n" + "=" * 80)
    print(" Data Summary (Network Partition)")
    print("=" * 80)
    print(f"{'Algorithm':<20} {'5 (A/B)':>12} {'10 (A/B)':>12} {'50 (A/B)':>12} {'100 (A/B)':>12}")
    print("-" * 80)

    for algo in ALGORITHMS:
        row = [ALGORITHM_LABELS[algo][:18]]
        for nc in NODE_COUNTS:
            result = load_partition_data(results_dir, algo, nc)
            if result:
                a_val = f"{result['partition_a']['mean']:.0f}" if 'partition_a' in result else "--"
                b_val = f"{result['partition_b']['mean']:.0f}" if 'partition_b' in result else "--"
                row.append(f"{a_val}/{b_val}")
            else:
                row.append("--/--")

        print(f"{row[0]:<20} {row[1]:>12} {row[2]:>12} {row[3]:>12} {row[4]:>12}")

    print("=" * 80)


# =============================================================================
# Main
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='Generate network partition comparison charts for leader election algorithms',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    # Combined 2x2 partition chart (both partitions)
    python3 plot_partition_comparison_charts.py -o partition_by_nodes.png

    # Only partition A
    python3 plot_partition_comparison_charts.py -p a -o partition_a_by_nodes.png

    # Separate charts
    python3 plot_partition_comparison_charts.py --separate -o ./charts/
        """
    )
    parser.add_argument('--results-dir', '-r', default='../../results',
                        help='Results directory (default: ../../results)')
    parser.add_argument('--output', '-o', default='partition_comparison.png',
                        help='Output file/directory (default: partition_comparison.png)')
    parser.add_argument('--partition', '-p', choices=['a', 'b', 'both'],
                        default='both',
                        help='Which partition to show (default: both)')
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

    script_dir = Path(__file__).parent
    results_dir = Path(args.results_dir)
    if not results_dir.is_absolute():
        results_dir = script_dir / results_dir
    results_dir = results_dir.resolve()

    print("=" * 70)
    print(" Network Partition Comparison Charts")
    print("=" * 70)
    print(f"Results directory: {results_dir}")
    print(f"Partition:        {args.partition}")
    print(f"Output format:    {args.format.upper()} @ {args.dpi} DPI")
    print(f"Mode:             {'Separate files' if args.separate else 'Combined 2x2 figure'}")

    if not results_dir.exists():
        print(f"\nError: Results directory not found: {results_dir}")
        print("\nTo generate data, run network partition experiments first.")
        sys.exit(1)

    if args.summary:
        print_data_summary(results_dir)

    print("\nGenerating charts...")

    if args.separate:
        output_dir = Path(args.output)
        if not output_dir.is_absolute():
            output_dir = script_dir / output_dir
        create_separate_figures(results_dir, output_dir, args.partition,
                               args.format, args.dpi)
    else:
        output_path = Path(args.output)
        if not output_path.is_absolute():
            output_path = script_dir / output_path
        create_combined_figure(results_dir, output_path, args.partition,
                              args.dpi, args.format, args.same_scale)

    print("\nDone!")


if __name__ == '__main__':
    main()
