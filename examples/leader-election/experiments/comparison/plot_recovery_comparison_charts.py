#!/usr/bin/env python3
"""
Leader Election Algorithm Fault Tolerance Comparison Charts by Node Count

Generates publication-quality comparison charts for crash recovery times across
all leader election algorithms, organized by network size (5, 10, 50, 100 nodes).

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
    python3 plot_recovery_comparison_charts.py [options]

Options:
    --results-dir, -r    Results directory (default: ../../results)
    --output, -o         Output file/directory (default: recovery_comparison.png)
    --separate           Save as 4 separate files instead of combined 2x2 figure
    --dpi                DPI for output (default: 300)
    --format, -f         Output format: png, pdf, svg (default: png)

Examples:
    # Combined 2x2 recovery time chart
    python3 plot_recovery_comparison_charts.py -o recovery_by_nodes.png

    # Separate charts
    python3 plot_recovery_comparison_charts.py --separate -o ./charts/

    # PDF for thesis
    python3 plot_recovery_comparison_charts.py -f pdf --dpi 600 -o thesis_recovery.pdf
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
    Find the most recent fault tolerance results directory for an algorithm/node combination.
    """
    possible_paths = [
        results_dir / algorithm / 'fault_tolerance',
    ]

    for base_path in possible_paths:
        if not base_path.exists():
            continue

        # Find directories matching the node count pattern
        dirs = sorted(
            [d for d in base_path.iterdir()
             if d.is_dir() and f'{node_count}nodes' in d.name],
            key=lambda x: x.name,
            reverse=True
        )

        if dirs:
            return dirs[0]

    return None


def load_recovery_data(results_dir: Path, algorithm: str, node_count: int) -> Optional[Dict]:
    """
    Load crash recovery time data for a specific algorithm and node count.

    Returns dict with: times, mean, std, median, min, max, count, ci_95
    """
    latest_dir = find_latest_results_dir(results_dir, algorithm, node_count)

    if not latest_dir:
        return None

    csv_file = latest_dir / 'crash_recovery_times.csv'
    if not csv_file.exists():
        return None

    times = []
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                # Try different column names
                time_val = None
                for col in ['total_recovery_time_ms', 'recovery_time_ms', 'recovery_convergence_time_ms']:
                    if col in row and row[col] != 'NA':
                        time_val = float(row[col])
                        break

                if time_val and time_val > 0:
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


# =============================================================================
# Plotting Functions
# =============================================================================

def plot_single_chart(ax, results_dir: Path, node_count: int) -> bool:
    """
    Plot a single comparison chart for a specific node count.
    """
    bar_width = 0.08  # Narrower bars to fit 10 algorithms
    x = np.arange(1)

    has_data = False

    for i, algo in enumerate(ALGORITHMS):
        result = load_recovery_data(results_dir, algo, node_count)

        if result:
            has_data = True
            mean = result['mean']
            std = result['std']
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

    ax.set_ylabel('Recovery Time (ms)', fontweight='bold')
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


def plot_single_chart_filtered(ax, results_dir: Path, node_count: int,
                                exclude_ring: bool = False,
                                exclude_bully: bool = False) -> bool:
    """
    Plot a single comparison chart for a specific node count.

    Args:
        ax: Matplotlib axis
        results_dir: Path to results directory
        node_count: Number of nodes
        exclude_ring: If True, exclude the ring algorithm from the chart
        exclude_bully: If True, exclude the bully algorithm from the chart
    """
    algos = [a for a in ALGORITHMS
             if not (exclude_ring and a == 'ring')
             and not (exclude_bully and a == 'bully')]
    # Adjust bar width based on number of algorithms
    num_algos = len(algos)
    bar_width = 0.08 if num_algos >= 9 else (0.09 if num_algos >= 8 else 0.1)
    x = np.arange(1)

    has_data = False

    for i, algo in enumerate(algos):
        result = load_recovery_data(results_dir, algo, node_count)

        if result:
            has_data = True
            mean = result['mean']
            std = result['std']
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

    ax.set_ylabel('Recovery Time (ms)', fontweight='bold')
    if exclude_ring and exclude_bully:
        suffix = " (excl. Ring & Bully)"
    elif exclude_ring:
        suffix = " (excl. Ring)"
    else:
        suffix = " (all algorithms)"
    ax.set_title(f'{node_count} Nodes{suffix}', fontweight='bold', fontsize=12)
    ax.set_xticks([])
    ax.set_axisbelow(True)
    ax.grid(True, axis='y', alpha=0.3)
    ax.set_ylim(bottom=0)

    if not has_data:
        ax.text(0.5, 0.5, 'No data available',
                transform=ax.transAxes, ha='center', va='center',
                fontsize=12, color='gray', style='italic')

    return has_data


def create_legend_handles_filtered(exclude_ring: bool = False, exclude_bully: bool = False):
    """Create legend handles for algorithms."""
    handles = []
    algos = [a for a in ALGORITHMS
             if not (exclude_ring and a == 'ring')
             and not (exclude_bully and a == 'bully')]
    for algo in algos:
        patch = mpatches.Patch(
            facecolor=ALGORITHM_COLORS[algo],
            edgecolor='black',
            hatch=ALGORITHM_HATCHES[algo],
            label=ALGORITHM_LABELS[algo]
        )
        handles.append(patch)
    return handles


def create_combined_figure_3x2(results_dir: Path, output_path: Path,
                               dpi: int = 300, format: str = 'png'):
    """Create a 4x3 figure comparing different algorithm exclusions."""
    fig, axes = plt.subplots(4, 3, figsize=(20, 18))

    fig.suptitle('Leader Election Crash Recovery Time Comparison\n(Left: All Algorithms, Middle: excl. Ring, Right: excl. Ring & Bully)',
                 fontsize=14, fontweight='bold', y=0.98)

    # Node counts to display (4 rows)
    node_counts_to_show = [5, 10, 50, 100]
    any_data = False

    for row, node_count in enumerate(node_counts_to_show):
        # Left column: all algorithms
        has_data = plot_single_chart_filtered(axes[row, 0], results_dir, node_count,
                                               exclude_ring=False, exclude_bully=False)
        any_data = any_data or has_data

        # Middle column: without ring
        has_data = plot_single_chart_filtered(axes[row, 1], results_dir, node_count,
                                               exclude_ring=True, exclude_bully=False)
        any_data = any_data or has_data

        # Right column: without ring and bully
        has_data = plot_single_chart_filtered(axes[row, 2], results_dir, node_count,
                                               exclude_ring=True, exclude_bully=True)
        any_data = any_data or has_data

    # Single legend with all algorithms
    handles_all = create_legend_handles_filtered(exclude_ring=False, exclude_bully=False)
    fig.legend(handles=handles_all, loc='lower center', ncol=5,
               bbox_to_anchor=(0.5, 0.01), framealpha=0.95,
               fontsize=9)

    plt.tight_layout(rect=[0, 0.06, 1, 0.95])

    if not output_path.suffix:
        output_path = output_path.with_suffix(f'.{format}')

    # Ensure output directory exists
    output_path.parent.mkdir(parents=True, exist_ok=True)

    plt.savefig(output_path, dpi=dpi, bbox_inches='tight', facecolor='white', format=format)
    plt.close()

    print(f"Combined 3x2 chart saved: {output_path}")

    if not any_data:
        print("\nWarning: No data was found for any algorithm/node combination.")
        print("Make sure you have run the fault tolerance experiments first.")

    return any_data


def create_combined_figure(results_dir: Path, output_path: Path,
                          dpi: int = 300, format: str = 'png'):
    """Create a 2x2 figure with all 4 node count charts."""
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))

    fig.suptitle('Leader Election Crash Recovery Time Comparison',
                 fontsize=14, fontweight='bold', y=0.98)

    axes_flat = axes.flatten()
    any_data = False

    for idx, node_count in enumerate(NODE_COUNTS):
        has_data = plot_single_chart(axes_flat[idx], results_dir, node_count)
        any_data = any_data or has_data

    handles = create_legend_handles()
    fig.legend(handles=handles, loc='lower center', ncol=5,
               bbox_to_anchor=(0.5, 0.02), framealpha=0.95,
               fontsize=9)

    plt.tight_layout(rect=[0, 0.08, 1, 0.96])

    if not output_path.suffix:
        output_path = output_path.with_suffix(f'.{format}')

    # Ensure output directory exists
    output_path.parent.mkdir(parents=True, exist_ok=True)

    plt.savefig(output_path, dpi=dpi, bbox_inches='tight', facecolor='white', format=format)
    plt.close()

    print(f"Combined chart saved: {output_path}")

    if not any_data:
        print("\nWarning: No data was found for any algorithm/node combination.")
        print("Make sure you have run the fault tolerance experiments first.")

    return any_data


def create_separate_figures(results_dir: Path, output_dir: Path,
                           format: str = 'png', dpi: int = 300):
    """Create 4 separate figures, one per node count."""
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    any_data = False

    for node_count in NODE_COUNTS:
        fig, ax = plt.subplots(figsize=(12, 7))

        has_data = plot_single_chart(ax, results_dir, node_count)
        any_data = any_data or has_data

        handles = create_legend_handles()
        ax.legend(handles=handles, loc='upper left', fontsize=9,
                 framealpha=0.95)

        ax.set_title(f'Crash Recovery Time Comparison - {node_count} Nodes',
                    fontweight='bold', fontsize=13)

        output_file = output_dir / f'recovery_comparison_{node_count}nodes.{format}'
        plt.tight_layout()
        plt.savefig(output_file, dpi=dpi, bbox_inches='tight', facecolor='white', format=format)
        plt.close()

        status = "saved" if has_data else "saved (no data)"
        print(f"Chart {status}: {output_file}")

    if not any_data:
        print("\nWarning: No data was found for any algorithm/node combination.")
        print("Make sure you have run the fault tolerance experiments first.")

    return any_data


def print_data_summary(results_dir: Path):
    """Print a summary of available data."""
    print("\n" + "=" * 70)
    print(" Data Summary (Recovery Time)")
    print("=" * 70)
    print(f"{'Algorithm':<20} {'5':>10} {'10':>10} {'50':>10} {'100':>10}")
    print("-" * 70)

    for algo in ALGORITHMS:
        row = [ALGORITHM_LABELS[algo][:18]]
        for nc in NODE_COUNTS:
            result = load_recovery_data(results_dir, algo, nc)
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
        description='Generate fault tolerance comparison charts for leader election algorithms',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    # Combined 2x2 recovery time chart
    python3 plot_recovery_comparison_charts.py -o recovery_by_nodes.png

    # Separate charts
    python3 plot_recovery_comparison_charts.py --separate -o ./charts/

    # PDF for thesis
    python3 plot_recovery_comparison_charts.py -f pdf --dpi 600 -o thesis_recovery.pdf
        """
    )
    parser.add_argument('--results-dir', '-r', default='../../results',
                        help='Results directory (default: ../../results)')
    parser.add_argument('--output', '-o', default='../../results/comparison_charts/recovery_comparison.png',
                        help='Output file/directory (default: ../../results/comparison_charts/recovery_comparison.png)')
    parser.add_argument('--separate', action='store_true',
                        help='Save as 4 separate files instead of combined')
    parser.add_argument('--grid-3x2', action='store_true',
                        help='Use 3x2 grid showing with/without ring algorithm comparison')
    parser.add_argument('--dpi', type=int, default=300,
                        help='DPI for output (default: 300)')
    parser.add_argument('--format', '-f', choices=['png', 'pdf', 'svg'],
                        default='png',
                        help='Output format (default: png)')
    parser.add_argument('--summary', action='store_true',
                        help='Print data summary table')

    args = parser.parse_args()

    script_dir = Path(__file__).parent
    results_dir = Path(args.results_dir)
    if not results_dir.is_absolute():
        results_dir = script_dir / results_dir
    results_dir = results_dir.resolve()

    print("=" * 70)
    print(" Fault Tolerance Comparison Charts")
    print("=" * 70)
    print(f"Results directory: {results_dir}")
    print(f"Output format:    {args.format.upper()} @ {args.dpi} DPI")
    mode = 'Separate files' if args.separate else ('Combined 3x2 figure (with/without ring)' if args.grid_3x2 else 'Combined 2x2 figure')
    print(f"Mode:             {mode}")

    if not results_dir.exists():
        print(f"\nError: Results directory not found: {results_dir}")
        print("\nTo generate data, run fault tolerance experiments first.")
        sys.exit(1)

    if args.summary:
        print_data_summary(results_dir)

    print("\nGenerating charts...")

    if args.separate:
        output_dir = Path(args.output)
        if not output_dir.is_absolute():
            output_dir = script_dir / output_dir
        create_separate_figures(results_dir, output_dir, args.format, args.dpi)
    elif args.grid_3x2:
        output_path = Path(args.output)
        if not output_path.is_absolute():
            output_path = script_dir / output_path
        create_combined_figure_3x2(results_dir, output_path, args.dpi, args.format)
    else:
        output_path = Path(args.output)
        if not output_path.is_absolute():
            output_path = script_dir / output_path
        create_combined_figure(results_dir, output_path, args.dpi, args.format)

    print("\nDone!")


if __name__ == '__main__':
    main()
