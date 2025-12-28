#!/usr/bin/env python3
"""
Bully Algorithm Network Partition Analysis Script

Analyzes partition trial results and generates statistical summaries and plots.

Usage:
    python3 analyze_partition.py -i partition_times.csv [-o output_dir] [--no-plots]

Input CSV format:
    trial,node_count,partition_a_convergence_ms,partition_b_convergence_ms,
    partition_a_leader,partition_b_leader,split_brain
"""

import argparse
import csv
import os
import statistics
import sys

# Try to import matplotlib for plotting
try:
    import matplotlib.pyplot as plt
    import matplotlib.patches as mpatches
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False


def load_data(csv_path):
    """Load partition trial data from CSV file."""
    data = {
        'trials': [],
        'node_count': None,
        'partition_a_times': [],
        'partition_b_times': [],
        'partition_a_leaders': [],
        'partition_b_leaders': [],
        'split_brain_count': 0
    }

    with open(csv_path, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            data['trials'].append(int(row['trial']))

            if data['node_count'] is None:
                data['node_count'] = int(row['node_count'])

            # Partition A convergence time
            if row['partition_a_convergence_ms'] != 'NA':
                data['partition_a_times'].append(float(row['partition_a_convergence_ms']))

            # Partition B convergence time
            if row['partition_b_convergence_ms'] != 'NA':
                data['partition_b_times'].append(float(row['partition_b_convergence_ms']))

            # Partition A leader
            if row['partition_a_leader'] != 'NA':
                data['partition_a_leaders'].append(int(row['partition_a_leader']))

            # Partition B leader
            if row['partition_b_leader'] != 'NA':
                data['partition_b_leaders'].append(int(row['partition_b_leader']))

            # Split-brain detection
            if row['split_brain'] == 'true':
                data['split_brain_count'] += 1

    return data


def get_partition_config(node_count):
    """Get partition configuration for a given node count."""
    configs = {
        5: {'a_range': (1, 2), 'b_range': (3, 5), 'a_expected': 2, 'b_expected': 5},
        10: {'a_range': (1, 5), 'b_range': (6, 10), 'a_expected': 5, 'b_expected': 10},
        50: {'a_range': (1, 25), 'b_range': (26, 50), 'a_expected': 25, 'b_expected': 50},
        100: {'a_range': (1, 50), 'b_range': (51, 100), 'a_expected': 50, 'b_expected': 100}
    }
    return configs.get(node_count, None)


def compute_statistics(values):
    """Compute summary statistics for a list of values."""
    if not values:
        return None

    stats = {
        'count': len(values),
        'mean': statistics.mean(values),
        'median': statistics.median(values),
        'min': min(values),
        'max': max(values),
        'range': max(values) - min(values)
    }

    if len(values) > 1:
        stats['stdev'] = statistics.stdev(values)
        stats['cv'] = (stats['stdev'] / stats['mean']) * 100 if stats['mean'] > 0 else 0
    else:
        stats['stdev'] = 0
        stats['cv'] = 0

    return stats


def print_report(data, config):
    """Print detailed analysis report to console."""
    total_trials = len(data['trials'])
    node_count = data['node_count']

    print()
    print("Network Partition Analysis:")
    print("=" * 70)
    print(f"Node Count:        {node_count}")
    print(f"Partition A:       Nodes {config['a_range'][0]}-{config['a_range'][1]} (expected leader: {config['a_expected']})")
    print(f"Partition B:       Nodes {config['b_range'][0]}-{config['b_range'][1]} (expected leader: {config['b_expected']})")
    print(f"Total Trials:      {total_trials}")
    print("=" * 70)
    print()

    # Partition A statistics
    a_stats = compute_statistics(data['partition_a_times'])
    print("Partition A Convergence Time:")
    print("-" * 70)
    if a_stats:
        print(f"  Valid Trials:    {a_stats['count']}/{total_trials}")
        print(f"  Mean:            {a_stats['mean']:.2f} ms ({a_stats['mean']/1000:.2f}s)")
        print(f"  Median:          {a_stats['median']:.2f} ms ({a_stats['median']/1000:.2f}s)")
        print(f"  Std Dev:         {a_stats['stdev']:.2f} ms")
        print(f"  Min:             {a_stats['min']:.2f} ms ({a_stats['min']/1000:.2f}s)")
        print(f"  Max:             {a_stats['max']:.2f} ms ({a_stats['max']/1000:.2f}s)")
        print(f"  Range:           {a_stats['range']:.2f} ms")
        print(f"  CV:              {a_stats['cv']:.2f}%")
    else:
        print("  No valid data")
    print()

    # Partition B statistics
    b_stats = compute_statistics(data['partition_b_times'])
    print("Partition B Convergence Time:")
    print("-" * 70)
    if b_stats:
        print(f"  Valid Trials:    {b_stats['count']}/{total_trials}")
        print(f"  Mean:            {b_stats['mean']:.2f} ms ({b_stats['mean']/1000:.2f}s)")
        print(f"  Median:          {b_stats['median']:.2f} ms ({b_stats['median']/1000:.2f}s)")
        print(f"  Std Dev:         {b_stats['stdev']:.2f} ms")
        print(f"  Min:             {b_stats['min']:.2f} ms ({b_stats['min']/1000:.2f}s)")
        print(f"  Max:             {b_stats['max']:.2f} ms ({b_stats['max']/1000:.2f}s)")
        print(f"  Range:           {b_stats['range']:.2f} ms")
        print(f"  CV:              {b_stats['cv']:.2f}%")
    else:
        print("  No valid data")
    print()

    # Leader election analysis
    print("Leader Election Analysis:")
    print("-" * 70)

    # Partition A leaders
    if data['partition_a_leaders']:
        a_leader_correct = sum(1 for l in data['partition_a_leaders'] if l == config['a_expected'])
        print(f"  Partition A Leader:")
        print(f"    Expected:      Node {config['a_expected']}")
        print(f"    Correct:       {a_leader_correct}/{len(data['partition_a_leaders'])} ({a_leader_correct*100/len(data['partition_a_leaders']):.1f}%)")
        unique_a = set(data['partition_a_leaders'])
        if len(unique_a) > 1:
            print(f"    Unique leaders: {sorted(unique_a)}")

    # Partition B leaders
    if data['partition_b_leaders']:
        b_leader_correct = sum(1 for l in data['partition_b_leaders'] if l == config['b_expected'])
        print(f"  Partition B Leader:")
        print(f"    Expected:      Node {config['b_expected']}")
        print(f"    Correct:       {b_leader_correct}/{len(data['partition_b_leaders'])} ({b_leader_correct*100/len(data['partition_b_leaders']):.1f}%)")
        unique_b = set(data['partition_b_leaders'])
        if len(unique_b) > 1:
            print(f"    Unique leaders: {sorted(unique_b)}")
    print()

    # Split-brain detection
    print("Split-Brain Detection:")
    print("-" * 70)
    split_pct = data['split_brain_count'] * 100 / total_trials if total_trials > 0 else 0
    print(f"  Trials with split-brain: {data['split_brain_count']}/{total_trials} ({split_pct:.1f}%)")
    print(f"  Expected: 100% (partitions should elect independent leaders)")
    if split_pct < 100:
        print(f"  WARNING: Some trials did not achieve split-brain state")
    print()
    print("=" * 70)

    return a_stats, b_stats


def save_summary_csv(data, config, a_stats, b_stats, output_path):
    """Save summary statistics to CSV file."""
    total_trials = len(data['trials'])

    with open(output_path, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['metric', 'partition_a', 'partition_b'])

        writer.writerow(['node_count', data['node_count'], data['node_count']])
        writer.writerow(['node_range', f"{config['a_range'][0]}-{config['a_range'][1]}",
                        f"{config['b_range'][0]}-{config['b_range'][1]}"])
        writer.writerow(['expected_leader', config['a_expected'], config['b_expected']])
        writer.writerow(['total_trials', total_trials, total_trials])

        if a_stats and b_stats:
            writer.writerow(['valid_trials', a_stats['count'], b_stats['count']])
            writer.writerow(['mean_ms', f"{a_stats['mean']:.2f}", f"{b_stats['mean']:.2f}"])
            writer.writerow(['median_ms', f"{a_stats['median']:.2f}", f"{b_stats['median']:.2f}"])
            writer.writerow(['stdev_ms', f"{a_stats['stdev']:.2f}", f"{b_stats['stdev']:.2f}"])
            writer.writerow(['min_ms', f"{a_stats['min']:.2f}", f"{b_stats['min']:.2f}"])
            writer.writerow(['max_ms', f"{a_stats['max']:.2f}", f"{b_stats['max']:.2f}"])
            writer.writerow(['cv_percent', f"{a_stats['cv']:.2f}", f"{b_stats['cv']:.2f}"])

        writer.writerow(['split_brain_count', data['split_brain_count'], data['split_brain_count']])
        writer.writerow(['split_brain_percent',
                        f"{data['split_brain_count']*100/total_trials:.1f}",
                        f"{data['split_brain_count']*100/total_trials:.1f}"])

    print(f"Summary CSV saved to: {output_path}")


def create_convergence_plot(data, config, a_stats, b_stats, output_path):
    """Create convergence time comparison plot."""
    if not HAS_MATPLOTLIB:
        print("Warning: matplotlib not available, skipping plots")
        return

    node_count = data['node_count']

    fig, axes = plt.subplots(1, 2, figsize=(14, 6))

    # Plot 1: Scatter plot of convergence times
    ax1 = axes[0]

    if data['partition_a_times']:
        trials_a = list(range(1, len(data['partition_a_times']) + 1))
        ax1.scatter(trials_a, data['partition_a_times'], alpha=0.6, s=20,
                   label=f'Partition A (nodes {config["a_range"][0]}-{config["a_range"][1]})', color='#2196F3')
        if a_stats:
            ax1.axhline(y=a_stats['mean'], color='#1565C0', linestyle='--', linewidth=2,
                       label=f'Partition A Mean: {a_stats["mean"]:.0f}ms')

    if data['partition_b_times']:
        trials_b = list(range(1, len(data['partition_b_times']) + 1))
        ax1.scatter(trials_b, data['partition_b_times'], alpha=0.6, s=20,
                   label=f'Partition B (nodes {config["b_range"][0]}-{config["b_range"][1]})', color='#FF9800')
        if b_stats:
            ax1.axhline(y=b_stats['mean'], color='#E65100', linestyle='--', linewidth=2,
                       label=f'Partition B Mean: {b_stats["mean"]:.0f}ms')

    ax1.set_xlabel('Trial Number', fontsize=12)
    ax1.set_ylabel('Convergence Time (ms)', fontsize=12)
    ax1.set_title(f'Network Partition Convergence Times\n({node_count} nodes)', fontsize=14)
    ax1.legend(loc='upper right', fontsize=9)
    ax1.grid(True, alpha=0.3)

    # Plot 2: Box plot comparison
    ax2 = axes[1]

    box_data = []
    box_labels = []
    box_colors = []

    if data['partition_a_times']:
        box_data.append(data['partition_a_times'])
        box_labels.append(f'Partition A\n(nodes {config["a_range"][0]}-{config["a_range"][1]})')
        box_colors.append('#2196F3')

    if data['partition_b_times']:
        box_data.append(data['partition_b_times'])
        box_labels.append(f'Partition B\n(nodes {config["b_range"][0]}-{config["b_range"][1]})')
        box_colors.append('#FF9800')

    if box_data:
        bp = ax2.boxplot(box_data, labels=box_labels, patch_artist=True)
        for patch, color in zip(bp['boxes'], box_colors):
            patch.set_facecolor(color)
            patch.set_alpha(0.6)

    ax2.set_ylabel('Convergence Time (ms)', fontsize=12)
    ax2.set_title(f'Convergence Time Distribution\n({node_count} nodes)', fontsize=14)
    ax2.grid(True, alpha=0.3, axis='y')

    # Add statistics text box
    if a_stats and b_stats:
        stats_text = (
            f"Partition A: μ={a_stats['mean']:.0f}ms, σ={a_stats['stdev']:.0f}ms\n"
            f"Partition B: μ={b_stats['mean']:.0f}ms, σ={b_stats['stdev']:.0f}ms\n"
            f"Split-brain: {data['split_brain_count']}/{len(data['trials'])} trials"
        )
        ax2.text(0.02, 0.98, stats_text, transform=ax2.transAxes, fontsize=10,
                verticalalignment='top', bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))

    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()

    print(f"Convergence plot saved to: {output_path}")


def create_leader_plot(data, config, output_path):
    """Create leader election distribution plot."""
    if not HAS_MATPLOTLIB:
        return

    node_count = data['node_count']

    fig, axes = plt.subplots(1, 2, figsize=(12, 5))

    # Partition A leader histogram
    ax1 = axes[0]
    if data['partition_a_leaders']:
        unique_leaders = sorted(set(data['partition_a_leaders']))
        counts = [data['partition_a_leaders'].count(l) for l in unique_leaders]
        colors = ['#4CAF50' if l == config['a_expected'] else '#F44336' for l in unique_leaders]
        ax1.bar([str(l) for l in unique_leaders], counts, color=colors, alpha=0.7)
        ax1.axhline(y=len(data['partition_a_leaders']), color='gray', linestyle=':', alpha=0.5)
    ax1.set_xlabel('Leader Node ID', fontsize=12)
    ax1.set_ylabel('Count', fontsize=12)
    ax1.set_title(f'Partition A Leader Distribution\n(expected: Node {config["a_expected"]})', fontsize=12)
    ax1.grid(True, alpha=0.3, axis='y')

    # Partition B leader histogram
    ax2 = axes[1]
    if data['partition_b_leaders']:
        unique_leaders = sorted(set(data['partition_b_leaders']))
        counts = [data['partition_b_leaders'].count(l) for l in unique_leaders]
        colors = ['#4CAF50' if l == config['b_expected'] else '#F44336' for l in unique_leaders]
        ax2.bar([str(l) for l in unique_leaders], counts, color=colors, alpha=0.7)
        ax2.axhline(y=len(data['partition_b_leaders']), color='gray', linestyle=':', alpha=0.5)
    ax2.set_xlabel('Leader Node ID', fontsize=12)
    ax2.set_ylabel('Count', fontsize=12)
    ax2.set_title(f'Partition B Leader Distribution\n(expected: Node {config["b_expected"]})', fontsize=12)
    ax2.grid(True, alpha=0.3, axis='y')

    # Add legend
    correct_patch = mpatches.Patch(color='#4CAF50', alpha=0.7, label='Correct Leader')
    incorrect_patch = mpatches.Patch(color='#F44336', alpha=0.7, label='Incorrect Leader')
    fig.legend(handles=[correct_patch, incorrect_patch], loc='upper center',
              ncol=2, bbox_to_anchor=(0.5, 0.02), fontsize=10)

    plt.tight_layout()
    plt.subplots_adjust(bottom=0.15)
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()

    print(f"Leader distribution plot saved to: {output_path}")


def main():
    parser = argparse.ArgumentParser(
        description='Analyze Bully algorithm network partition trial results'
    )
    parser.add_argument('-i', '--input', required=True,
                       help='Input CSV file (partition_times.csv)')
    parser.add_argument('-o', '--output', default=None,
                       help='Output directory for plots and summary (default: same as input)')
    parser.add_argument('--no-plots', action='store_true',
                       help='Skip generating plots (statistics only)')

    args = parser.parse_args()

    # Validate input file
    if not os.path.exists(args.input):
        print(f"Error: Input file not found: {args.input}")
        sys.exit(1)

    # Set output directory
    output_dir = args.output if args.output else os.path.dirname(args.input)
    if output_dir and not os.path.exists(output_dir):
        os.makedirs(output_dir)

    # Load data
    print(f"Loading data from: {args.input}")
    data = load_data(args.input)

    if not data['trials']:
        print("Error: No trial data found in CSV file")
        sys.exit(1)

    # Get partition configuration
    config = get_partition_config(data['node_count'])
    if not config:
        print(f"Error: Unknown node count {data['node_count']}")
        sys.exit(1)

    # Print report and get statistics
    a_stats, b_stats = print_report(data, config)

    # Save summary CSV
    summary_path = os.path.join(output_dir, 'partition_summary.csv')
    save_summary_csv(data, config, a_stats, b_stats, summary_path)

    # Generate plots
    if not args.no_plots and HAS_MATPLOTLIB:
        node_count = data['node_count']

        # Convergence time plot
        conv_plot_path = os.path.join(output_dir, f'partition_convergence_{node_count}nodes.png')
        create_convergence_plot(data, config, a_stats, b_stats, conv_plot_path)

        # Leader distribution plot
        leader_plot_path = os.path.join(output_dir, f'partition_leaders_{node_count}nodes.png')
        create_leader_plot(data, config, leader_plot_path)
    elif not args.no_plots and not HAS_MATPLOTLIB:
        print("Warning: matplotlib not installed, skipping plots")
        print("Install with: pip install matplotlib")

    print()
    valid_a = len(data['partition_a_times'])
    valid_b = len(data['partition_b_times'])
    print(f"Success! Analyzed {len(data['trials'])} trials.")
    print(f"Valid partition A measurements: {valid_a}/{len(data['trials'])}")
    print(f"Valid partition B measurements: {valid_b}/{len(data['trials'])}")


if __name__ == '__main__':
    main()
