#!/usr/bin/env python3
"""
Leader Election Experiment Dashboard Generator

Generates a single-page summary dashboard with all key metrics from leader
election experiments. Ideal for thesis/publication figures.

The dashboard includes:
  - Convergence time comparison (bar chart)
  - Scalability analysis (line plot)
  - Message overhead comparison (bar chart)
  - State distribution (pie charts)
  - Summary statistics table

Usage:
    python3 generate_dashboard.py [options]

Options:
    --results-dir, -r    Results directory (default: ../results)
    --output, -o         Output file path (default: dashboard.png)
    --dpi                DPI for output (default: 300)
    --format             Output format: png, pdf, svg (default: png)

Examples:
    python3 generate_dashboard.py
    python3 generate_dashboard.py -r ../results -o thesis_dashboard.png
    python3 generate_dashboard.py --format pdf --dpi 600
"""

import argparse
import csv
from pathlib import Path
import sys
from typing import Dict, Optional

try:
    import matplotlib.pyplot as plt
    import matplotlib.gridspec as gridspec
    import numpy as np
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("Error: matplotlib is required for dashboard generation")
    print("Install with: pip install matplotlib numpy")
    sys.exit(1)

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
    'bully': '#1f77b4',
    'ring': '#ff7f0e',
    'prasle': '#2ca02c',
    'adaptive-prasle': '#d62728'
}

ALGORITHM_MARKERS = {
    'bully': 'o',
    'ring': 's',
    'prasle': '^',
    'adaptive-prasle': 'D'
}

NODE_COUNTS = [5, 10, 50, 100]

STATE_COLORS = {
    'normal': '#4CAF50',
    'election': '#FF9800',
    'waiting': '#2196F3'
}

# =============================================================================
# Data Loading Functions
# =============================================================================

def load_convergence_results(results_dir: Path, algorithm: str, node_count: int) -> Optional[Dict]:
    """Load convergence results for a specific algorithm and node count."""
    results_path = results_dir / algorithm / 'convergence_trials'

    if not results_path.exists():
        return None

    dirs = sorted(
        [d for d in results_path.iterdir() if d.is_dir() and f'{node_count}nodes' in d.name],
        key=lambda x: x.name, reverse=True
    )

    if not dirs:
        return None

    csv_file = dirs[0] / 'convergence_times.csv'
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
        'count': len(times)
    }


def load_metrics_results(results_dir: Path, algorithm: str, node_count: int) -> Optional[Dict]:
    """Load detailed metrics from trial directories."""
    results_path = results_dir / algorithm / 'convergence_trials'

    if not results_path.exists():
        return None

    dirs = sorted(
        [d for d in results_path.iterdir() if d.is_dir() and f'{node_count}nodes' in d.name],
        key=lambda x: x.name, reverse=True
    )

    if not dirs:
        return None

    latest_dir = dirs[0]
    total_messages = []
    elections_started = []
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
            if rows:
                last_row = rows[-1]
                try:
                    total_messages.append(int(last_row.get('messages_sent', 0)))
                    elections_started.append(int(last_row.get('elections_started', 0)))
                    time_normal.append(float(last_row.get('time_in_normal_ms', 0)))
                    time_election.append(float(last_row.get('time_in_election_ms', 0)))
                    time_waiting.append(float(last_row.get('time_in_waiting_ms', 0)))
                except (KeyError, ValueError):
                    continue

    if not total_messages:
        return None

    return {
        'messages': {'mean': np.mean(total_messages), 'std': np.std(total_messages)},
        'elections': {'mean': np.mean(elections_started), 'std': np.std(elections_started)},
        'state_distribution': {
            'normal': np.mean(time_normal) if time_normal else 0,
            'election': np.mean(time_election) if time_election else 0,
            'waiting': np.mean(time_waiting) if time_waiting else 0
        }
    }


# =============================================================================
# Dashboard Generation
# =============================================================================

def generate_dashboard(results_dir: Path, output_path: Path, dpi: int = 300):
    """
    Generate a comprehensive dashboard with all key metrics.

    Creates a single figure with multiple subplots showing:
    - Convergence time comparison
    - Scalability analysis
    - Message overhead
    - State distribution
    """
    print("=" * 60)
    print(" Leader Election Experiment Dashboard")
    print("=" * 60)

    # Set up figure with GridSpec for flexible layout
    fig = plt.figure(figsize=(16, 12))
    gs = gridspec.GridSpec(3, 3, figure=fig, hspace=0.35, wspace=0.3)

    # Title
    fig.suptitle('Leader Election Algorithm Comparison Dashboard',
                 fontsize=16, fontweight='bold', y=0.98)

    # =========================================================================
    # Panel 1: Convergence Time Bar Chart (top-left, 2 cols)
    # =========================================================================
    ax1 = fig.add_subplot(gs[0, :2])

    bar_width = 0.2
    x = np.arange(len(NODE_COUNTS))
    has_conv_data = False

    for i, algo in enumerate(ALGORITHMS):
        means = []
        stds = []
        for nc in NODE_COUNTS:
            result = load_convergence_results(results_dir, algo, nc)
            if result:
                has_conv_data = True
                means.append(result['mean'])
                stds.append(result['std'])
            else:
                means.append(0)
                stds.append(0)

        ax1.bar(x + i * bar_width, means, bar_width,
               label=ALGORITHM_LABELS[algo], color=ALGORITHM_COLORS[algo],
               yerr=stds, capsize=3, alpha=0.85)

    ax1.set_xlabel('Number of Nodes', fontweight='bold')
    ax1.set_ylabel('Convergence Time (ms)', fontweight='bold')
    ax1.set_title('Convergence Time Comparison', fontweight='bold')
    ax1.set_xticks(x + bar_width * 1.5)
    ax1.set_xticklabels(NODE_COUNTS)
    ax1.legend(loc='upper left', fontsize=9)
    ax1.grid(True, alpha=0.3, axis='y')

    if not has_conv_data:
        ax1.text(0.5, 0.5, 'No data available', transform=ax1.transAxes,
                ha='center', va='center', fontsize=12, color='gray')

    # =========================================================================
    # Panel 2: Summary Stats Box (top-right)
    # =========================================================================
    ax2 = fig.add_subplot(gs[0, 2])
    ax2.axis('off')

    # Create summary text
    summary_text = "Summary Statistics\n" + "=" * 25 + "\n\n"

    for algo in ALGORITHMS:
        result = load_convergence_results(results_dir, algo, 10)
        if result:
            summary_text += f"{ALGORITHM_LABELS[algo]:15}\n"
            summary_text += f"  Mean: {result['mean']:.1f} ms\n"
            summary_text += f"  Std:  {result['std']:.1f} ms\n"
            summary_text += f"  n:    {result['count']}\n\n"

    ax2.text(0.1, 0.95, summary_text, transform=ax2.transAxes,
            fontsize=10, fontfamily='monospace', verticalalignment='top',
            bbox=dict(boxstyle='round', facecolor='white', edgecolor='gray', alpha=0.9))

    # =========================================================================
    # Panel 3: Scalability Line Plot (middle-left, 2 cols)
    # =========================================================================
    ax3 = fig.add_subplot(gs[1, :2])

    has_scale_data = False
    for algo in ALGORITHMS:
        node_counts = []
        means = []
        stds = []

        for nc in NODE_COUNTS:
            result = load_convergence_results(results_dir, algo, nc)
            if result:
                has_scale_data = True
                node_counts.append(nc)
                means.append(result['mean'])
                stds.append(result['std'])

        if node_counts:
            ax3.errorbar(node_counts, means, yerr=stds,
                        label=ALGORITHM_LABELS[algo], color=ALGORITHM_COLORS[algo],
                        marker=ALGORITHM_MARKERS[algo], markersize=8,
                        linewidth=2, capsize=4)

    ax3.set_xlabel('Number of Nodes', fontweight='bold')
    ax3.set_ylabel('Convergence Time (ms)', fontweight='bold')
    ax3.set_title('Scalability: Convergence Time vs Network Size', fontweight='bold')
    ax3.set_xticks(NODE_COUNTS)
    ax3.legend(loc='upper left', fontsize=9)
    ax3.grid(True, alpha=0.3)

    if not has_scale_data:
        ax3.text(0.5, 0.5, 'No data available', transform=ax3.transAxes,
                ha='center', va='center', fontsize=12, color='gray')

    # =========================================================================
    # Panel 4: State Distribution Pie Charts (middle-right)
    # =========================================================================
    ax4 = fig.add_subplot(gs[1, 2])

    # Show state distribution for first algorithm with data (10 nodes)
    state_algo = None
    state_data = None
    for algo in ALGORITHMS:
        result = load_metrics_results(results_dir, algo, 10)
        if result and 'state_distribution' in result:
            sd = result['state_distribution']
            total = sd['normal'] + sd['election'] + sd['waiting']
            if total > 0:
                state_algo = algo
                state_data = [sd['normal'], sd['election'], sd['waiting']]
                break

    if state_data:
        colors = [STATE_COLORS['normal'], STATE_COLORS['election'], STATE_COLORS['waiting']]
        labels = ['Normal', 'Election', 'Waiting']
        ax4.pie(state_data, labels=labels, colors=colors, autopct='%1.1f%%',
               startangle=90, textprops={'fontsize': 9})
        ax4.set_title(f'State Distribution\n({ALGORITHM_LABELS[state_algo]}, 10 nodes)',
                     fontweight='bold')
    else:
        ax4.axis('off')
        ax4.text(0.5, 0.5, 'No state data\navailable', transform=ax4.transAxes,
                ha='center', va='center', fontsize=12, color='gray')

    # =========================================================================
    # Panel 5: Message Overhead Comparison (bottom-left, 2 cols)
    # =========================================================================
    ax5 = fig.add_subplot(gs[2, :2])

    has_msg_data = False
    for i, algo in enumerate(ALGORITHMS):
        means = []
        stds = []
        for nc in NODE_COUNTS:
            result = load_metrics_results(results_dir, algo, nc)
            if result and 'messages' in result:
                has_msg_data = True
                means.append(result['messages']['mean'])
                stds.append(result['messages']['std'])
            else:
                means.append(0)
                stds.append(0)

        ax5.bar(x + i * bar_width, means, bar_width,
               label=ALGORITHM_LABELS[algo], color=ALGORITHM_COLORS[algo],
               yerr=stds, capsize=3, alpha=0.85)

    ax5.set_xlabel('Number of Nodes', fontweight='bold')
    ax5.set_ylabel('Total Messages Sent', fontweight='bold')
    ax5.set_title('Message Overhead Comparison', fontweight='bold')
    ax5.set_xticks(x + bar_width * 1.5)
    ax5.set_xticklabels(NODE_COUNTS)
    ax5.legend(loc='upper left', fontsize=9)
    ax5.grid(True, alpha=0.3, axis='y')

    if not has_msg_data:
        ax5.text(0.5, 0.5, 'No data available', transform=ax5.transAxes,
                ha='center', va='center', fontsize=12, color='gray')

    # =========================================================================
    # Panel 6: Algorithm Comparison Table (bottom-right)
    # =========================================================================
    ax6 = fig.add_subplot(gs[2, 2])
    ax6.axis('off')

    # Create comparison table
    table_data = []
    table_cols = ['Algorithm', 'Conv.\n(10 nodes)', 'Msgs']

    for algo in ALGORITHMS:
        conv_result = load_convergence_results(results_dir, algo, 10)
        metrics_result = load_metrics_results(results_dir, algo, 10)

        conv_str = f"{conv_result['mean']:.0f}" if conv_result else "N/A"
        msg_str = f"{metrics_result['messages']['mean']:.0f}" if metrics_result else "N/A"

        table_data.append([ALGORITHM_LABELS[algo], conv_str, msg_str])

    if table_data:
        table = ax6.table(cellText=table_data, colLabels=table_cols,
                         loc='center', cellLoc='center',
                         colColours=['#f0f0f0'] * 3)
        table.auto_set_font_size(False)
        table.set_fontsize(10)
        table.scale(1.2, 1.5)
        ax6.set_title('Quick Comparison (10 nodes)', fontweight='bold', y=0.85)

    # =========================================================================
    # Save figure
    # =========================================================================
    plt.savefig(output_path, dpi=dpi, bbox_inches='tight', facecolor='white')
    plt.close()

    print(f"\nDashboard saved: {output_path}")
    print(f"Resolution: {dpi} DPI")


def main():
    parser = argparse.ArgumentParser(
        description='Generate leader election experiment dashboard',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument('--results-dir', '-r', default='../results',
                        help='Results directory (default: ../results)')
    parser.add_argument('--output', '-o', default='dashboard.png',
                        help='Output file path (default: dashboard.png)')
    parser.add_argument('--dpi', type=int, default=300,
                        help='DPI for output (default: 300)')
    parser.add_argument('--format', '-f', choices=['png', 'pdf', 'svg'], default='png',
                        help='Output format (default: png)')

    args = parser.parse_args()

    results_dir = Path(args.results_dir)
    output_path = Path(args.output)

    # Ensure output has correct extension
    if not output_path.suffix:
        output_path = output_path.with_suffix(f'.{args.format}')

    if not results_dir.exists():
        print(f"Error: Results directory not found: {results_dir}")
        print("Run experiments first using run_all_experiments.sh")
        sys.exit(1)

    generate_dashboard(results_dir, output_path, args.dpi)


if __name__ == '__main__':
    main()
