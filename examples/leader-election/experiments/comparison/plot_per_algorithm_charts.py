#!/usr/bin/env python3
"""
Per-Algorithm Metrics Charts for Leader Election Experiments

Generates separate graphs for each algorithm showing metrics (messages sent,
convergence time, etc.) across different numbers of nodes.

This creates one multi-panel figure per algorithm containing:
  1. Convergence Time vs Node Count
  2. Messages Sent vs Node Count
  3. Combined scalability view

Usage:
    python3 plot_per_algorithm_charts.py [options]

Options:
    --results-dir, -r    Results directory (default: ../../results)
    --output, -o         Output directory (default: ./per_algorithm_charts)
    --algorithm, -a      Specific algorithm to plot (default: all)
    --dpi                DPI for output (default: 300)
    --format, -f         Output format: png, pdf, svg (default: png)

Examples:
    # Generate charts for all algorithms
    python3 plot_per_algorithm_charts.py

    # Generate chart for bully algorithm only
    python3 plot_per_algorithm_charts.py -a bully

    # High quality PDF for thesis
    python3 plot_per_algorithm_charts.py -f pdf --dpi 600 -o ./thesis_charts/
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
    'bully': 'Bully Algorithm',
    'ring': 'Ring Algorithm',
    'prasle': 'PraSLE (Clique Topology)',
    'prasle-line': 'PraSLE (Line Topology)',
    'prasle-ring': 'PraSLE (Ring Topology)',
    'prasle-mesh': 'PraSLE (Mesh Topology)',
    'adaptive-prasle': 'Adaptive-PraSLE (Clique Topology)',
    'adaptive-prasle-line': 'Adaptive-PraSLE (Line Topology)',
    'adaptive-prasle-ring': 'Adaptive-PraSLE (Ring Topology)',
    'adaptive-prasle-mesh': 'Adaptive-PraSLE (Mesh Topology)'
}

ALGORITHM_COLORS = {
    'bully': '#1f77b4',           # Blue
    'ring': '#ff7f0e',            # Orange
    'prasle': '#2ca02c',          # Dark green
    'prasle-line': '#006400',     # Forest green
    'prasle-ring': '#90ee90',     # Light green
    'prasle-mesh': '#98df8a',     # Pale green
    'adaptive-prasle': '#d62728',          # Red
    'adaptive-prasle-line': '#8b0000',     # Dark red
    'adaptive-prasle-ring': '#ff6b6b',     # Light red/coral
    'adaptive-prasle-mesh': '#ffb6c1'      # Light pink
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
        'median': np.median(times),
        'min': min(times),
        'max': max(times),
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
        'median': np.median(total_messages),
        'min': min(total_messages),
        'max': max(total_messages),
        'count': len(total_messages),
        'ci_95': 1.96 * np.std(total_messages) / np.sqrt(len(total_messages))
    }


def load_elections_data(results_dir: Path, algorithm: str, node_count: int) -> Optional[Dict]:
    """Load elections started data for a specific algorithm and node count."""
    latest_dir = find_latest_results_dir(results_dir, algorithm, node_count)

    if not latest_dir:
        return None

    elections_started = []

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
                        elections = int(row.get('elections_started', 0))

                        if node_id not in node_final_data or ts > node_final_data[node_id][0]:
                            node_final_data[node_id] = (ts, elections)
                    except (KeyError, ValueError):
                        continue

                if node_final_data:
                    trial_total = sum(elections for ts, elections in node_final_data.values())
                    if trial_total >= 0:
                        elections_started.append(trial_total)

    if not elections_started:
        return None

    return {
        'values': elections_started,
        'mean': np.mean(elections_started),
        'std': np.std(elections_started),
        'count': len(elections_started)
    }


def load_leader_changes_data(results_dir: Path, algorithm: str, node_count: int) -> Optional[Dict]:
    """Load leader changes data for a specific algorithm and node count."""
    latest_dir = find_latest_results_dir(results_dir, algorithm, node_count)

    if not latest_dir:
        return None

    leader_changes = []

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
                        changes = int(row.get('leader_changes', 0))

                        if node_id not in node_final_data or ts > node_final_data[node_id][0]:
                            node_final_data[node_id] = (ts, changes)
                    except (KeyError, ValueError):
                        continue

                if node_final_data:
                    trial_total = sum(changes for ts, changes in node_final_data.values())
                    if trial_total >= 0:
                        leader_changes.append(trial_total)

    if not leader_changes:
        return None

    return {
        'values': leader_changes,
        'mean': np.mean(leader_changes),
        'std': np.std(leader_changes),
        'count': len(leader_changes)
    }


# =============================================================================
# Plotting Functions
# =============================================================================

def plot_algorithm_metrics(results_dir: Path, algorithm: str, output_dir: Path,
                           format: str = 'png', dpi: int = 300) -> Optional[Path]:
    """
    Generate a multi-panel figure for a single algorithm showing all metrics
    across different node counts.

    Layout (2x2):
        +---------------------------+---------------------------+
        | Convergence Time vs Nodes | Messages Sent vs Nodes    |
        +---------------------------+---------------------------+
        | Elections Started vs Nodes| Leader Changes vs Nodes   |
        +---------------------------+---------------------------+
    """
    color = ALGORITHM_COLORS.get(algorithm, '#1f77b4')
    label = ALGORITHM_LABELS.get(algorithm, algorithm)

    # Collect data for all node counts
    conv_data = []
    msg_data = []
    elec_data = []
    leader_data = []
    valid_nodes = []

    for nc in NODE_COUNTS:
        conv = load_convergence_data(results_dir, algorithm, nc)
        msg = load_message_data(results_dir, algorithm, nc)
        elec = load_elections_data(results_dir, algorithm, nc)
        leader = load_leader_changes_data(results_dir, algorithm, nc)

        if conv or msg:  # At least some data available
            valid_nodes.append(nc)
            conv_data.append(conv)
            msg_data.append(msg)
            elec_data.append(elec)
            leader_data.append(leader)

    if not valid_nodes:
        print(f"  No data found for {algorithm}")
        return None

    # Create figure with 2x2 subplots
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle(f'{label}\nMetrics Across Network Sizes', fontsize=14, fontweight='bold', y=0.98)

    # Plot 1: Convergence Time
    ax1 = axes[0, 0]
    conv_means = [d['mean'] if d else 0 for d in conv_data]
    conv_stds = [d['std'] if d else 0 for d in conv_data]
    conv_valid = [i for i, d in enumerate(conv_data) if d]

    if conv_valid:
        x_pos = np.arange(len(valid_nodes))
        bars = ax1.bar(x_pos, conv_means, color=color, alpha=0.8, edgecolor='black', linewidth=0.5)
        ax1.errorbar(x_pos, conv_means, yerr=conv_stds, fmt='none', color='black', capsize=5, capthick=1.5)

        # Add value labels on bars
        for i, (bar, mean, std) in enumerate(zip(bars, conv_means, conv_stds)):
            if mean > 0:
                ax1.text(bar.get_x() + bar.get_width()/2, bar.get_height() + std + 50,
                        f'{mean:.0f}', ha='center', va='bottom', fontsize=9, fontweight='bold')

    ax1.set_xlabel('Number of Nodes', fontweight='bold')
    ax1.set_ylabel('Convergence Time (ms)', fontweight='bold')
    ax1.set_title('Convergence Time', fontweight='bold', fontsize=12)
    ax1.set_xticks(np.arange(len(valid_nodes)))
    ax1.set_xticklabels(valid_nodes)
    ax1.set_ylim(bottom=0)
    ax1.grid(True, axis='y', alpha=0.3)
    ax1.set_axisbelow(True)

    # Plot 2: Messages Sent
    ax2 = axes[0, 1]
    msg_means = [d['mean'] if d else 0 for d in msg_data]
    msg_stds = [d['std'] if d else 0 for d in msg_data]
    msg_valid = [i for i, d in enumerate(msg_data) if d]

    if msg_valid:
        x_pos = np.arange(len(valid_nodes))
        bars = ax2.bar(x_pos, msg_means, color=color, alpha=0.8, edgecolor='black', linewidth=0.5)
        ax2.errorbar(x_pos, msg_means, yerr=msg_stds, fmt='none', color='black', capsize=5, capthick=1.5)

        for i, (bar, mean, std) in enumerate(zip(bars, msg_means, msg_stds)):
            if mean > 0:
                ax2.text(bar.get_x() + bar.get_width()/2, bar.get_height() + std + 5,
                        f'{mean:.0f}', ha='center', va='bottom', fontsize=9, fontweight='bold')

    ax2.set_xlabel('Number of Nodes', fontweight='bold')
    ax2.set_ylabel('Total Messages Sent', fontweight='bold')
    ax2.set_title('Message Overhead', fontweight='bold', fontsize=12)
    ax2.set_xticks(np.arange(len(valid_nodes)))
    ax2.set_xticklabels(valid_nodes)
    ax2.set_ylim(bottom=0)
    ax2.grid(True, axis='y', alpha=0.3)
    ax2.set_axisbelow(True)

    # Plot 3: Elections Started
    ax3 = axes[1, 0]
    elec_means = [d['mean'] if d else 0 for d in elec_data]
    elec_stds = [d['std'] if d else 0 for d in elec_data]
    elec_valid = [i for i, d in enumerate(elec_data) if d]

    if elec_valid:
        x_pos = np.arange(len(valid_nodes))
        bars = ax3.bar(x_pos, elec_means, color=color, alpha=0.8, edgecolor='black', linewidth=0.5)
        ax3.errorbar(x_pos, elec_means, yerr=elec_stds, fmt='none', color='black', capsize=5, capthick=1.5)

        for i, (bar, mean, std) in enumerate(zip(bars, elec_means, elec_stds)):
            if mean > 0:
                ax3.text(bar.get_x() + bar.get_width()/2, bar.get_height() + std + 0.5,
                        f'{mean:.1f}', ha='center', va='bottom', fontsize=9, fontweight='bold')

    ax3.set_xlabel('Number of Nodes', fontweight='bold')
    ax3.set_ylabel('Elections Started', fontweight='bold')
    ax3.set_title('Election Count', fontweight='bold', fontsize=12)
    ax3.set_xticks(np.arange(len(valid_nodes)))
    ax3.set_xticklabels(valid_nodes)
    ax3.set_ylim(bottom=0)
    ax3.grid(True, axis='y', alpha=0.3)
    ax3.set_axisbelow(True)

    # Plot 4: Leader Changes
    ax4 = axes[1, 1]
    leader_means = [d['mean'] if d else 0 for d in leader_data]
    leader_stds = [d['std'] if d else 0 for d in leader_data]
    leader_valid = [i for i, d in enumerate(leader_data) if d]

    if leader_valid:
        x_pos = np.arange(len(valid_nodes))
        bars = ax4.bar(x_pos, leader_means, color=color, alpha=0.8, edgecolor='black', linewidth=0.5)
        ax4.errorbar(x_pos, leader_means, yerr=leader_stds, fmt='none', color='black', capsize=5, capthick=1.5)

        for i, (bar, mean, std) in enumerate(zip(bars, leader_means, leader_stds)):
            if mean > 0:
                ax4.text(bar.get_x() + bar.get_width()/2, bar.get_height() + std + 0.5,
                        f'{mean:.1f}', ha='center', va='bottom', fontsize=9, fontweight='bold')

    ax4.set_xlabel('Number of Nodes', fontweight='bold')
    ax4.set_ylabel('Leader Changes', fontweight='bold')
    ax4.set_title('Leader Stability', fontweight='bold', fontsize=12)
    ax4.set_xticks(np.arange(len(valid_nodes)))
    ax4.set_xticklabels(valid_nodes)
    ax4.set_ylim(bottom=0)
    ax4.grid(True, axis='y', alpha=0.3)
    ax4.set_axisbelow(True)

    plt.tight_layout(rect=[0, 0, 1, 0.96])

    # Save figure
    output_file = output_dir / f'{algorithm}_metrics.{format}'
    plt.savefig(output_file, dpi=dpi, bbox_inches='tight', facecolor='white', format=format)
    plt.close()

    print(f"  Saved: {output_file}")
    return output_file


def plot_algorithm_scalability(results_dir: Path, algorithm: str, output_dir: Path,
                                format: str = 'png', dpi: int = 300) -> Optional[Path]:
    """
    Generate a line plot showing scalability trends for a single algorithm.

    Shows convergence time and messages sent on dual y-axes as node count increases.
    """
    color = ALGORITHM_COLORS.get(algorithm, '#1f77b4')
    label = ALGORITHM_LABELS.get(algorithm, algorithm)

    # Collect data
    conv_data = []
    msg_data = []
    valid_nodes = []

    for nc in NODE_COUNTS:
        conv = load_convergence_data(results_dir, algorithm, nc)
        msg = load_message_data(results_dir, algorithm, nc)

        if conv or msg:
            valid_nodes.append(nc)
            conv_data.append(conv)
            msg_data.append(msg)

    if not valid_nodes:
        return None

    # Create figure with dual y-axes
    fig, ax1 = plt.subplots(figsize=(10, 6))
    ax2 = ax1.twinx()

    # Plot convergence time (left y-axis)
    conv_means = [d['mean'] if d else None for d in conv_data]
    conv_stds = [d['std'] if d else 0 for d in conv_data]

    valid_conv_nodes = [n for n, m in zip(valid_nodes, conv_means) if m is not None]
    valid_conv_means = [m for m in conv_means if m is not None]
    valid_conv_stds = [s for m, s in zip(conv_means, conv_stds) if m is not None]

    if valid_conv_nodes:
        line1 = ax1.errorbar(valid_conv_nodes, valid_conv_means, yerr=valid_conv_stds,
                             color=color, marker='o', markersize=10, linewidth=2.5,
                             capsize=5, capthick=1.5, label='Convergence Time')
        ax1.fill_between(valid_conv_nodes,
                         np.array(valid_conv_means) - np.array(valid_conv_stds),
                         np.array(valid_conv_means) + np.array(valid_conv_stds),
                         color=color, alpha=0.2)

    # Plot messages sent (right y-axis)
    msg_means = [d['mean'] if d else None for d in msg_data]
    msg_stds = [d['std'] if d else 0 for d in msg_data]

    valid_msg_nodes = [n for n, m in zip(valid_nodes, msg_means) if m is not None]
    valid_msg_means = [m for m in msg_means if m is not None]
    valid_msg_stds = [s for m, s in zip(msg_means, msg_stds) if m is not None]

    if valid_msg_nodes:
        darker_color = '#8b4513' if color == '#ff7f0e' else '#2c5aa0'  # Avoid similar colors
        line2 = ax2.errorbar(valid_msg_nodes, valid_msg_means, yerr=valid_msg_stds,
                             color=darker_color, marker='s', markersize=10, linewidth=2.5,
                             linestyle='--', capsize=5, capthick=1.5, label='Messages Sent')

    # Configure axes
    ax1.set_xlabel('Number of Nodes', fontweight='bold', fontsize=12)
    ax1.set_ylabel('Convergence Time (ms)', fontweight='bold', fontsize=12, color=color)
    ax1.tick_params(axis='y', labelcolor=color)
    ax1.set_xticks(NODE_COUNTS)
    ax1.set_ylim(bottom=0)
    ax1.grid(True, alpha=0.3)
    ax1.set_axisbelow(True)

    ax2.set_ylabel('Messages Sent', fontweight='bold', fontsize=12, color=darker_color)
    ax2.tick_params(axis='y', labelcolor=darker_color)
    ax2.set_ylim(bottom=0)

    # Title and legend
    plt.title(f'{label}\nScalability Analysis', fontsize=14, fontweight='bold', pad=15)

    # Combine legends
    lines1, labels1 = ax1.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    ax1.legend(lines1 + lines2, labels1 + labels2, loc='upper left', framealpha=0.95)

    plt.tight_layout()

    # Save figure
    output_file = output_dir / f'{algorithm}_scalability.{format}'
    plt.savefig(output_file, dpi=dpi, bbox_inches='tight', facecolor='white', format=format)
    plt.close()

    print(f"  Saved: {output_file}")
    return output_file


def print_algorithm_summary(results_dir: Path, algorithm: str):
    """Print a summary of data for an algorithm."""
    label = ALGORITHM_LABELS.get(algorithm, algorithm)
    print(f"\n  {label}")
    print(f"  {'-' * 60}")
    print(f"  {'Nodes':<8} {'Conv. (ms)':<15} {'Messages':<12} {'Elections':<12} {'Leader Chg':<12}")
    print(f"  {'-' * 60}")

    for nc in NODE_COUNTS:
        conv = load_convergence_data(results_dir, algorithm, nc)
        msg = load_message_data(results_dir, algorithm, nc)
        elec = load_elections_data(results_dir, algorithm, nc)
        leader = load_leader_changes_data(results_dir, algorithm, nc)

        conv_str = f"{conv['mean']:.0f} +/- {conv['std']:.0f}" if conv else "N/A"
        msg_str = f"{msg['mean']:.0f}" if msg else "N/A"
        elec_str = f"{elec['mean']:.1f}" if elec else "N/A"
        leader_str = f"{leader['mean']:.1f}" if leader else "N/A"

        print(f"  {nc:<8} {conv_str:<15} {msg_str:<12} {elec_str:<12} {leader_str:<12}")


# =============================================================================
# Main
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='Generate per-algorithm metrics charts for leader election experiments',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    # Generate charts for all algorithms
    python3 plot_per_algorithm_charts.py

    # Generate chart for bully algorithm only
    python3 plot_per_algorithm_charts.py -a bully

    # High quality PDF for thesis
    python3 plot_per_algorithm_charts.py -f pdf --dpi 600 -o ./thesis_charts/
        """
    )
    parser.add_argument('--results-dir', '-r', default='../../results',
                        help='Results directory (default: ../../results)')
    parser.add_argument('--output', '-o', default='../../results/comparison_charts/per_algorithm',
                        help='Output directory (default: ../../results/comparison_charts/per_algorithm)')
    parser.add_argument('--algorithm', '-a', default='all',
                        help='Specific algorithm to plot, or "all" (default: all)')
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

    output_dir = Path(args.output)
    if not output_dir.is_absolute():
        output_dir = script_dir / output_dir
    output_dir = output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    print("=" * 70)
    print(" Per-Algorithm Metrics Charts")
    print("=" * 70)
    print(f"Results directory: {results_dir}")
    print(f"Output directory:  {output_dir}")
    print(f"Output format:     {args.format.upper()} @ {args.dpi} DPI")

    if not results_dir.exists():
        print(f"\nError: Results directory not found: {results_dir}")
        sys.exit(1)

    # Determine which algorithms to process
    if args.algorithm.lower() == 'all':
        algorithms_to_process = ALGORITHMS
    else:
        if args.algorithm not in ALGORITHMS:
            print(f"\nError: Unknown algorithm '{args.algorithm}'")
            print(f"Available algorithms: {', '.join(ALGORITHMS)}")
            sys.exit(1)
        algorithms_to_process = [args.algorithm]

    print(f"Algorithms:        {len(algorithms_to_process)} selected")

    # Print summary if requested
    if args.summary:
        print("\n" + "=" * 70)
        print(" Data Summary")
        print("=" * 70)
        for algo in algorithms_to_process:
            print_algorithm_summary(results_dir, algo)

    # Generate charts
    print("\n" + "=" * 70)
    print(" Generating Charts")
    print("=" * 70)

    generated_files = []
    for algo in algorithms_to_process:
        print(f"\nProcessing: {ALGORITHM_LABELS.get(algo, algo)}")

        # Generate metrics chart (2x2 panel)
        result = plot_algorithm_metrics(results_dir, algo, output_dir, args.format, args.dpi)
        if result:
            generated_files.append(result)

        # Generate scalability chart
        result = plot_algorithm_scalability(results_dir, algo, output_dir, args.format, args.dpi)
        if result:
            generated_files.append(result)

    # Summary
    print("\n" + "=" * 70)
    print(" Complete")
    print("=" * 70)
    print(f"\nOutput directory: {output_dir}")

    if generated_files:
        print(f"\nGenerated {len(generated_files)} files:")
        for f in generated_files:
            print(f"  - {f.name}")
    else:
        print("\nNo files were generated (no data available).")
        print("Make sure you have run experiments first.")


if __name__ == '__main__':
    main()
