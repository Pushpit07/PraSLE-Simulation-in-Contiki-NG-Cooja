#!/usr/bin/env python3
"""
Leader Election Algorithm Energy Consumption Charts

Generates publication-quality energy consumption comparison charts for all
leader election algorithms, using Energest counter data from metrics CSV files.

Charts generated:
  1. Total energy consumption per algorithm across network sizes
  2. Energy breakdown (TX vs RX vs CPU vs LPM) per algorithm
  3. Radio duty cycle comparison across algorithms

Usage:
    python3 plot_energy_charts.py [options]

Options:
    --results-dir, -r    Results directory (default: ../../results)
    --output-dir, -o     Output directory (default: current directory)
    --case, -c           Parameter case: case1, case2 (default: case2)
    --dpi                DPI for output (default: 300)
    --format, -f         Output format: png, pdf, svg (default: png)
"""

import argparse
import csv
import os
from pathlib import Path
import sys
from typing import Dict, List, Optional

try:
    import matplotlib.pyplot as plt
    import matplotlib.patches as mpatches
    import numpy as np
except ImportError:
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
    'prasle-ring': '#90ee90',
    'prasle-mesh': '#98df8a',
    'adaptive-prasle': '#d62728',
    'adaptive-prasle-line': '#8b0000',
    'adaptive-prasle-ring': '#ff6b6b',
    'adaptive-prasle-mesh': '#ffb6c1'
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

# Contiki-NG clock ticks per second (platform dependent, typically 128 for sky motes)
# Adjust this if using a different platform
RTIMER_SECOND = 32768  # For CC2650/CC2538 platforms
CLOCK_SECOND = 128     # Contiki-NG default

# Energy model constants (typical CC2650 values in mW)
POWER_CPU_MW = 5.4      # CPU active
POWER_LPM_MW = 0.0054   # Low-power mode
POWER_TX_MW = 24.0       # Radio transmit (0 dBm)
POWER_RX_MW = 20.0       # Radio receive/listen

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


def load_energy_data(results_dir: Path, algorithm: str, node_count: int) -> Optional[Dict]:
    """
    Load energy consumption data for a specific algorithm and node count.

    For each trial, reads the final metrics row for each node and extracts
    Energest counters (energest_cpu, energest_lpm, energest_tx, energest_rx).

    Returns dict with per-trial totals for each energy component.
    """
    latest_dir = find_latest_results_dir(results_dir, algorithm, node_count)

    if not latest_dir:
        return None

    trial_cpu = []
    trial_lpm = []
    trial_tx = []
    trial_rx = []
    trial_total = []

    for trial_dir in latest_dir.iterdir():
        if not trial_dir.is_dir():
            continue

        metrics_file = trial_dir / 'metrics.csv'
        if not metrics_file.exists():
            continue

        with open(metrics_file, 'r') as f:
            reader = csv.DictReader(f)
            rows = list(reader)

            if not rows:
                continue

            # Group by node_id and keep the latest timestamp for each node
            node_final_data = {}
            for row in rows:
                try:
                    node_id = int(row.get('node_id', 0))
                    ts = int(row.get('timestamp', 0))
                    cpu = int(row.get('energest_cpu', 0))
                    lpm = int(row.get('energest_lpm', 0))
                    tx = int(row.get('energest_tx', 0))
                    rx = int(row.get('energest_rx', 0))

                    if node_id not in node_final_data or ts > node_final_data[node_id][0]:
                        node_final_data[node_id] = (ts, cpu, lpm, tx, rx)
                except (KeyError, ValueError):
                    continue

            if node_final_data:
                # Sum across all nodes in this trial
                total_cpu = sum(d[1] for d in node_final_data.values())
                total_lpm = sum(d[2] for d in node_final_data.values())
                total_tx = sum(d[3] for d in node_final_data.values())
                total_rx = sum(d[4] for d in node_final_data.values())

                if total_cpu + total_lpm + total_tx + total_rx > 0:
                    trial_cpu.append(total_cpu)
                    trial_lpm.append(total_lpm)
                    trial_tx.append(total_tx)
                    trial_rx.append(total_rx)
                    trial_total.append(total_cpu + total_tx + total_rx)  # Exclude LPM from "active" energy

    if not trial_total:
        return None

    return {
        'cpu': {'values': trial_cpu, 'mean': np.mean(trial_cpu), 'std': np.std(trial_cpu)},
        'lpm': {'values': trial_lpm, 'mean': np.mean(trial_lpm), 'std': np.std(trial_lpm)},
        'tx': {'values': trial_tx, 'mean': np.mean(trial_tx), 'std': np.std(trial_tx)},
        'rx': {'values': trial_rx, 'mean': np.mean(trial_rx), 'std': np.std(trial_rx)},
        'total_active': {'values': trial_total, 'mean': np.mean(trial_total), 'std': np.std(trial_total)},
        'count': len(trial_total),
        'duty_cycle': {
            'tx_mean': np.mean(trial_tx) / (np.mean(trial_cpu) + np.mean(trial_lpm)) * 100 if (np.mean(trial_cpu) + np.mean(trial_lpm)) > 0 else 0,
            'rx_mean': np.mean(trial_rx) / (np.mean(trial_cpu) + np.mean(trial_lpm)) * 100 if (np.mean(trial_cpu) + np.mean(trial_lpm)) > 0 else 0,
        }
    }


def ticks_to_energy_mj(ticks: float, power_mw: float) -> float:
    """Convert Energest ticks to energy in millijoules."""
    time_s = ticks / RTIMER_SECOND
    return power_mw * time_s


# =============================================================================
# Plotting Functions
# =============================================================================

def plot_total_energy(results_dir: Path, output_dir: Path, fmt: str, dpi: int):
    """Plot total energy consumption per algorithm across network sizes."""
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle('Total Energy Consumption by Network Size', fontweight='bold')

    for idx, node_count in enumerate(NODE_COUNTS):
        ax = axes[idx // 2][idx % 2]
        bar_width = 0.08
        has_data = False

        for i, algo in enumerate(ALGORITHMS):
            data = load_energy_data(results_dir, algo, node_count)
            if data is None:
                continue

            # Convert ticks to millijoules
            cpu_mj = ticks_to_energy_mj(data['cpu']['mean'], POWER_CPU_MW)
            tx_mj = ticks_to_energy_mj(data['tx']['mean'], POWER_TX_MW)
            rx_mj = ticks_to_energy_mj(data['rx']['mean'], POWER_RX_MW)
            total_mj = cpu_mj + tx_mj + rx_mj

            x_pos = i * bar_width
            ax.bar(x_pos, total_mj,
                   width=bar_width * 0.85,
                   color=ALGORITHM_COLORS.get(algo, '#333333'),
                   hatch=ALGORITHM_HATCHES.get(algo, ''),
                   edgecolor='black', linewidth=0.5,
                   label=ALGORITHM_LABELS.get(algo, algo))
            has_data = True

        ax.set_title(f'{node_count} Nodes')
        ax.set_ylabel('Energy (mJ)')
        ax.set_xticks([i * bar_width for i in range(len(ALGORITHMS))])
        ax.set_xticklabels([ALGORITHM_LABELS.get(a, a) for a in ALGORITHMS],
                           rotation=45, ha='right', fontsize=8)

        if not has_data:
            ax.text(0.5, 0.5, 'No data available', transform=ax.transAxes,
                    ha='center', va='center', fontsize=12, color='gray')

    plt.tight_layout()
    output_file = output_dir / f'energy_total_by_nodes.{fmt}'
    plt.savefig(output_file, dpi=dpi, bbox_inches='tight')
    print(f"Saved: {output_file}")
    plt.close()


def plot_energy_breakdown(results_dir: Path, output_dir: Path, fmt: str, dpi: int):
    """Plot energy breakdown (CPU, TX, RX) per algorithm for 100 nodes."""
    fig, ax = plt.subplots(figsize=(14, 7))
    fig.suptitle('Energy Breakdown by Component (100 Nodes)', fontweight='bold')

    bar_width = 0.6
    x_positions = []
    labels = []

    for i, algo in enumerate(ALGORITHMS):
        data = load_energy_data(results_dir, algo, 100)
        if data is None:
            continue

        cpu_mj = ticks_to_energy_mj(data['cpu']['mean'], POWER_CPU_MW)
        tx_mj = ticks_to_energy_mj(data['tx']['mean'], POWER_TX_MW)
        rx_mj = ticks_to_energy_mj(data['rx']['mean'], POWER_RX_MW)

        x_pos = len(x_positions)
        x_positions.append(x_pos)
        labels.append(ALGORITHM_LABELS.get(algo, algo))

        ax.bar(x_pos, cpu_mj, width=bar_width, color='#4CAF50', label='CPU' if x_pos == 0 else '')
        ax.bar(x_pos, tx_mj, width=bar_width, bottom=cpu_mj, color='#FF5722', label='TX' if x_pos == 0 else '')
        ax.bar(x_pos, rx_mj, width=bar_width, bottom=cpu_mj + tx_mj, color='#2196F3', label='RX' if x_pos == 0 else '')

    ax.set_ylabel('Energy (mJ)')
    ax.set_xticks(x_positions)
    ax.set_xticklabels(labels, rotation=45, ha='right')
    ax.legend()

    plt.tight_layout()
    output_file = output_dir / f'energy_breakdown_100nodes.{fmt}'
    plt.savefig(output_file, dpi=dpi, bbox_inches='tight')
    print(f"Saved: {output_file}")
    plt.close()


def plot_radio_duty_cycle(results_dir: Path, output_dir: Path, fmt: str, dpi: int):
    """Plot radio duty cycle (TX + RX percentage) across algorithms and network sizes."""
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle('Radio Duty Cycle by Network Size', fontweight='bold')

    for idx, node_count in enumerate(NODE_COUNTS):
        ax = axes[idx // 2][idx % 2]
        bar_width = 0.08
        has_data = False

        for i, algo in enumerate(ALGORITHMS):
            data = load_energy_data(results_dir, algo, node_count)
            if data is None:
                continue

            tx_pct = data['duty_cycle']['tx_mean']
            rx_pct = data['duty_cycle']['rx_mean']

            x_pos = i * bar_width
            ax.bar(x_pos, rx_pct, width=bar_width * 0.85,
                   color='#2196F3', edgecolor='black', linewidth=0.5,
                   label='RX' if i == 0 else '')
            ax.bar(x_pos, tx_pct, width=bar_width * 0.85, bottom=rx_pct,
                   color='#FF5722', edgecolor='black', linewidth=0.5,
                   label='TX' if i == 0 else '')
            has_data = True

        ax.set_title(f'{node_count} Nodes')
        ax.set_ylabel('Radio Duty Cycle (%)')
        ax.set_xticks([i * bar_width for i in range(len(ALGORITHMS))])
        ax.set_xticklabels([ALGORITHM_LABELS.get(a, a) for a in ALGORITHMS],
                           rotation=45, ha='right', fontsize=8)

        if not has_data:
            ax.text(0.5, 0.5, 'No data available', transform=ax.transAxes,
                    ha='center', va='center', fontsize=12, color='gray')
        elif idx == 0:
            ax.legend()

    plt.tight_layout()
    output_file = output_dir / f'radio_duty_cycle_by_nodes.{fmt}'
    plt.savefig(output_file, dpi=dpi, bbox_inches='tight')
    print(f"Saved: {output_file}")
    plt.close()


def print_energy_summary(results_dir: Path):
    """Print a text summary of energy consumption for all algorithms."""
    print("\n" + "=" * 80)
    print("ENERGY CONSUMPTION SUMMARY")
    print("=" * 80)

    for node_count in NODE_COUNTS:
        print(f"\n--- {node_count} Nodes ---")
        print(f"{'Algorithm':<25} {'CPU (mJ)':>10} {'TX (mJ)':>10} {'RX (mJ)':>10} {'Total (mJ)':>12} {'Trials':>8}")
        print("-" * 80)

        for algo in ALGORITHMS:
            data = load_energy_data(results_dir, algo, node_count)
            if data is None:
                print(f"{ALGORITHM_LABELS.get(algo, algo):<25} {'N/A':>10}")
                continue

            cpu_mj = ticks_to_energy_mj(data['cpu']['mean'], POWER_CPU_MW)
            tx_mj = ticks_to_energy_mj(data['tx']['mean'], POWER_TX_MW)
            rx_mj = ticks_to_energy_mj(data['rx']['mean'], POWER_RX_MW)
            total_mj = cpu_mj + tx_mj + rx_mj

            print(f"{ALGORITHM_LABELS.get(algo, algo):<25} {cpu_mj:>10.1f} {tx_mj:>10.1f} {rx_mj:>10.1f} {total_mj:>12.1f} {data['count']:>8}")


# =============================================================================
# Main
# =============================================================================

def main():
    parser = argparse.ArgumentParser(description='Generate energy consumption charts')
    parser.add_argument('--results-dir', '-r', type=str, default='../../results',
                        help='Results directory (default: ../../results)')
    parser.add_argument('--output-dir', '-o', type=str, default='.',
                        help='Output directory (default: current directory)')
    parser.add_argument('--case', '-c', type=str, default='case2',
                        help='Parameter case: case1, case2 (default: case2)')
    parser.add_argument('--dpi', type=int, default=300,
                        help='DPI for output (default: 300)')
    parser.add_argument('--format', '-f', type=str, default='png',
                        choices=['png', 'pdf', 'svg'],
                        help='Output format (default: png)')
    parser.add_argument('--summary', action='store_true',
                        help='Print text summary of energy data')

    args = parser.parse_args()

    results_dir = Path(args.results_dir) / args.case
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    if not results_dir.exists():
        print(f"Error: Results directory not found: {results_dir}")
        sys.exit(1)

    print(f"Loading results from: {results_dir}")
    print(f"Output directory: {output_dir}")

    if args.summary:
        print_energy_summary(results_dir)

    print("\nGenerating energy charts...")
    plot_total_energy(results_dir, output_dir, args.format, args.dpi)
    plot_energy_breakdown(results_dir, output_dir, args.format, args.dpi)
    plot_radio_duty_cycle(results_dir, output_dir, args.format, args.dpi)

    print("\nDone!")


if __name__ == '__main__':
    main()
