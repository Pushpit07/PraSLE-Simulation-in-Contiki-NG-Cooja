#!/usr/bin/env python3
"""
Bully Algorithm Leader Crash Recovery Analysis

This script analyzes and visualizes leader crash recovery data from multiple trials,
showing initial convergence times, recovery times, and statistical analysis.

Usage:
    python3 analyze_recovery.py -i <csv_file> [-o <output_dir>]

Parameters:
    -i, --input      Path to crash_recovery_times.csv file (required)
    -o, --output     Output directory for plots and analysis (default: current directory)
    --dpi            DPI for output images (default: 300)
    --no-plots       Skip generating plots (statistics only)
"""

import argparse
import csv
import statistics
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


def load_recovery_data(csv_file):
    """
    Load crash recovery data from CSV file.

    Args:
        csv_file: Path to CSV file with columns:
                  trial, node_count, crash_time_s, initial_convergence_time_ms,
                  recovery_convergence_time_ms, total_recovery_time_ms

    Returns:
        tuple: (trials, node_count, crash_time, initial_times, recovery_times, total_recovery_times)
    """
    trials = []
    initial_times = []
    recovery_times = []
    total_recovery_times = []
    node_count = None
    crash_time = None

    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            trial_num = int(row['trial'])
            node_count = int(row['node_count'])
            crash_time = int(row['crash_time_s'])

            trials.append(trial_num)

            # Initial convergence time
            if row['initial_convergence_time_ms'] != 'NA':
                initial_times.append(float(row['initial_convergence_time_ms']))
            else:
                initial_times.append(None)

            # Recovery convergence time (absolute time)
            if row['recovery_convergence_time_ms'] != 'NA':
                recovery_times.append(float(row['recovery_convergence_time_ms']))
            else:
                recovery_times.append(None)

            # Total recovery time (crash to re-election)
            if row['total_recovery_time_ms'] != 'NA':
                total_recovery_times.append(float(row['total_recovery_time_ms']))
            else:
                total_recovery_times.append(None)

    if not trials:
        raise ValueError("No trial data found in CSV file")

    return trials, node_count, crash_time, initial_times, recovery_times, total_recovery_times


def calculate_statistics(data):
    """
    Calculate statistical measures for the data, filtering out None values.

    Args:
        data: List of numerical values (may contain None)

    Returns:
        dict: Dictionary containing statistical measures
    """
    # Filter out None values
    valid_data = [x for x in data if x is not None]

    if not valid_data:
        return {
            'mean': 0,
            'median': 0,
            'stdev': 0,
            'min': 0,
            'max': 0,
            'range': 0,
            'count': 0,
            'valid_count': 0,
            'cv': 0
        }

    return {
        'mean': statistics.mean(valid_data),
        'median': statistics.median(valid_data),
        'stdev': statistics.stdev(valid_data) if len(valid_data) > 1 else 0,
        'min': min(valid_data),
        'max': max(valid_data),
        'range': max(valid_data) - min(valid_data),
        'count': len(data),
        'valid_count': len(valid_data),
        'cv': (statistics.stdev(valid_data) / statistics.mean(valid_data) * 100) if len(valid_data) > 1 else 0
    }


def print_statistics(initial_stats, recovery_stats, node_count, crash_time):
    """
    Print formatted statistics table to console.

    Args:
        initial_stats: Dictionary of initial convergence statistics
        recovery_stats: Dictionary of recovery time statistics
        node_count: Number of nodes in the experiment
        crash_time: Time when leader was crashed (seconds)
    """
    print("\nLeader Crash Recovery Analysis:")
    print("=" * 70)
    print(f"Node Count:        {node_count}")
    print(f"Crash Time:        {crash_time}s")
    print(f"Total Trials:      {initial_stats['count']}")
    print("=" * 70)
    print()

    print("Initial Convergence (before crash):")
    print("-" * 70)
    if initial_stats['valid_count'] > 0:
        print(f"  Valid Trials:    {initial_stats['valid_count']}/{initial_stats['count']}")
        print(f"  Mean:            {initial_stats['mean']:.2f} ms")
        print(f"  Median:          {initial_stats['median']:.2f} ms")
        print(f"  Std Dev:         {initial_stats['stdev']:.2f} ms")
        print(f"  Min:             {initial_stats['min']:.2f} ms")
        print(f"  Max:             {initial_stats['max']:.2f} ms")
        print(f"  Range:           {initial_stats['range']:.2f} ms")
        print(f"  CV:              {initial_stats['cv']:.2f}%")
    else:
        print("  No valid data")
    print()

    print("Recovery Time (crash to re-election):")
    print("-" * 70)
    if recovery_stats['valid_count'] > 0:
        print(f"  Valid Trials:    {recovery_stats['valid_count']}/{recovery_stats['count']}")
        print(f"  Mean:            {recovery_stats['mean']:.2f} ms ({recovery_stats['mean']/1000:.2f}s)")
        print(f"  Median:          {recovery_stats['median']:.2f} ms ({recovery_stats['median']/1000:.2f}s)")
        print(f"  Std Dev:         {recovery_stats['stdev']:.2f} ms")
        print(f"  Min:             {recovery_stats['min']:.2f} ms ({recovery_stats['min']/1000:.2f}s)")
        print(f"  Max:             {recovery_stats['max']:.2f} ms ({recovery_stats['max']/1000:.2f}s)")
        print(f"  Range:           {recovery_stats['range']:.2f} ms")
        print(f"  CV:              {recovery_stats['cv']:.2f}%")
        print()
        print(f"  Expected Detection Time: ~20s (COORDINATOR_TIMEOUT)")
        print(f"  Observed Detection+Re-election: {recovery_stats['mean']/1000:.2f}s")
    else:
        print("  No valid data - check simulation logs")
    print()

    print("=" * 70)


def plot_recovery_timeline(trials, initial_times, recovery_times, total_recovery_times,
                           crash_time, stats_initial, stats_recovery,
                           node_count, output_file, dpi=300):
    """
    Create timeline plot showing initial convergence, crash, and recovery.

    Args:
        trials: List of trial numbers
        initial_times: List of initial convergence times (ms)
        recovery_times: List of recovery convergence times (ms, absolute)
        total_recovery_times: List of recovery durations (ms, relative to crash)
        crash_time: Time when leader crashed (seconds)
        stats_initial: Statistics for initial convergence
        stats_recovery: Statistics for recovery time
        node_count: Number of nodes
        output_file: Path to save the PNG file
        dpi: DPI for output image
    """
    plt.figure(figsize=(14, 8))

    # Filter valid data points
    valid_trials = []
    valid_initial = []
    valid_recovery = []

    for i, trial in enumerate(trials):
        if initial_times[i] is not None and recovery_times[i] is not None:
            valid_trials.append(trial)
            valid_initial.append(initial_times[i])
            valid_recovery.append(recovery_times[i])

    if not valid_trials:
        print("Warning: No valid recovery data to plot")
        return

    crash_time_ms = crash_time * 1000

    # Plot initial convergence points
    plt.scatter(valid_trials, valid_initial, alpha=0.6, s=40,
                color='green', label='Initial Convergence', zorder=3, marker='o')

    # Plot recovery convergence points
    plt.scatter(valid_trials, valid_recovery, alpha=0.6, s=40,
                color='red', label='Recovery Convergence', zorder=3, marker='s')

    # Plot crash time line
    plt.axhline(y=crash_time_ms, color='orange', linestyle='--', linewidth=2,
                label=f'Leader Crash Time: {crash_time}s', zorder=2)

    # Plot mean lines
    if stats_initial['valid_count'] > 0:
        plt.axhline(y=stats_initial['mean'], color='darkgreen', linestyle=':', linewidth=1.5,
                    label=f'Mean Initial: {stats_initial["mean"]:.1f}ms', zorder=1)

    if stats_recovery['valid_count'] > 0:
        mean_recovery_absolute = crash_time_ms + stats_recovery['mean']
        plt.axhline(y=mean_recovery_absolute, color='darkred', linestyle=':', linewidth=1.5,
                    label=f'Mean Recovery Time: {stats_recovery["mean"]:.1f}ms', zorder=1)

    # Configure plot
    plt.title(f'Leader Crash Recovery Timeline - {node_count} Nodes ({len(valid_trials)} trials)',
              fontsize=14, fontweight='bold', pad=20)
    plt.xlabel('Trial Number', fontsize=12)
    plt.ylabel('Time (ms)', fontsize=12)
    plt.grid(True, alpha=0.3, linestyle=':', zorder=0)
    plt.legend(loc='best', fontsize=9, framealpha=0.9)

    # Tight layout
    plt.tight_layout()

    # Save figure
    plt.savefig(output_file, dpi=dpi, bbox_inches='tight')
    print(f"Timeline plot saved to: {output_file}")
    plt.close()


def plot_recovery_distribution(trials, total_recovery_times, stats_recovery,
                                node_count, crash_time, output_file, dpi=300):
    """
    Create distribution plot showing recovery time variation.

    Args:
        trials: List of trial numbers
        total_recovery_times: List of recovery durations (ms)
        stats_recovery: Statistics for recovery time
        node_count: Number of nodes
        crash_time: Time when leader crashed (seconds)
        output_file: Path to save the PNG file
        dpi: DPI for output image
    """
    # Filter valid data
    valid_trials = []
    valid_recovery = []

    for i, trial in enumerate(trials):
        if total_recovery_times[i] is not None:
            valid_trials.append(trial)
            valid_recovery.append(total_recovery_times[i])

    if not valid_trials:
        print("Warning: No valid recovery data to plot")
        return

    plt.figure(figsize=(12, 7))
    ax = plt.gca()

    # Plot individual measurements
    plt.scatter(valid_trials, valid_recovery, alpha=0.6, s=30,
                color='steelblue', label='Recovery Time', zorder=3)

    # Plot mean line
    mean_val = stats_recovery['mean']
    plt.axhline(y=mean_val, color='red', linestyle='--', linewidth=2,
                label=f'Mean: {mean_val:.1f}ms ({mean_val/1000:.1f}s)', zorder=4)

    # Plot ±1σ shaded region
    std_val = stats_recovery['stdev']
    plt.axhspan(mean_val - std_val, mean_val + std_val,
                alpha=0.2, color='gray', label=f'±1σ: {std_val:.1f}ms', zorder=1)

    # Configure plot
    plt.title(f'Recovery Time Distribution - {node_count} Nodes (Crash at {crash_time}s)',
              fontsize=14, fontweight='bold', pad=20)
    plt.xlabel('Trial Number', fontsize=12)
    plt.ylabel('Recovery Time (ms)', fontsize=12)
    plt.grid(True, alpha=0.3, linestyle=':', zorder=0)
    plt.legend(loc='best', fontsize=10, framealpha=0.9)

    # Set axis limits with padding
    y_min = min(valid_recovery)
    y_max = max(valid_recovery)
    y_padding = (y_max - y_min) * 0.1
    plt.ylim(y_min - y_padding, y_max + y_padding)

    x_padding = max(valid_trials) * 0.02
    plt.xlim(0 - x_padding, max(valid_trials) + x_padding)

    # Add statistics text box
    textstr = '\n'.join((
        f'Recovery Time Statistics:',
        f'Mean = {mean_val:.1f}ms ({mean_val/1000:.1f}s)',
        f'Median = {stats_recovery["median"]:.1f}ms',
        f'Std Dev = {std_val:.1f}ms',
        f'Min = {stats_recovery["min"]:.1f}ms',
        f'Max = {stats_recovery["max"]:.1f}ms',
        f'CV = {stats_recovery["cv"]:.1f}%'
    ))
    props = dict(boxstyle='round', facecolor='wheat', alpha=0.8)
    ax.text(0.02, 0.98, textstr, transform=ax.transAxes, fontsize=9,
            verticalalignment='top', bbox=props, family='monospace')

    plt.tight_layout()
    plt.savefig(output_file, dpi=dpi, bbox_inches='tight')
    print(f"Distribution plot saved to: {output_file}")
    plt.close()


def save_summary_csv(output_dir, initial_stats, recovery_stats, node_count, crash_time):
    """
    Save summary statistics to CSV file.

    Args:
        output_dir: Output directory
        initial_stats: Initial convergence statistics
        recovery_stats: Recovery time statistics
        node_count: Number of nodes
        crash_time: Crash time in seconds
    """
    summary_file = output_dir / 'recovery_summary.csv'

    with open(summary_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['Metric', 'Initial_Convergence_ms', 'Recovery_Time_ms'])
        writer.writerow(['Node_Count', node_count, node_count])
        writer.writerow(['Crash_Time_s', '-', crash_time])
        writer.writerow(['Valid_Trials', initial_stats['valid_count'], recovery_stats['valid_count']])
        writer.writerow(['Mean', f"{initial_stats['mean']:.2f}", f"{recovery_stats['mean']:.2f}"])
        writer.writerow(['Median', f"{initial_stats['median']:.2f}", f"{recovery_stats['median']:.2f}"])
        writer.writerow(['Std_Dev', f"{initial_stats['stdev']:.2f}", f"{recovery_stats['stdev']:.2f}"])
        writer.writerow(['Min', f"{initial_stats['min']:.2f}", f"{recovery_stats['min']:.2f}"])
        writer.writerow(['Max', f"{initial_stats['max']:.2f}", f"{recovery_stats['max']:.2f}"])
        writer.writerow(['CV_%', f"{initial_stats['cv']:.2f}", f"{recovery_stats['cv']:.2f}"])

    print(f"Summary CSV saved to: {summary_file}")


def main():
    """Main function to parse arguments and generate analysis."""
    parser = argparse.ArgumentParser(
        description='Analyze leader crash recovery data from trial results',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s -i results/fault_tolerance/50nodes_crash60s/crash_recovery_times.csv
  %(prog)s -i data.csv -o analysis_output/
  %(prog)s -i data.csv --no-plots
        """
    )

    parser.add_argument('-i', '--input', required=True,
                        help='Path to crash_recovery_times.csv file')
    parser.add_argument('-o', '--output', default='.',
                        help='Output directory for plots and analysis (default: current directory)')
    parser.add_argument('--dpi', type=int, default=300,
                        help='DPI for output images (default: 300)')
    parser.add_argument('--no-plots', action='store_true',
                        help='Skip generating plots (statistics only)')

    args = parser.parse_args()

    # Check if input file exists
    input_path = Path(args.input)
    if not input_path.exists():
        print(f"Error: Input file not found: {args.input}", file=sys.stderr)
        sys.exit(1)

    # Create output directory
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    try:
        # Load data
        print(f"Loading data from: {args.input}")
        trials, node_count, crash_time, initial_times, recovery_times, total_recovery_times = \
            load_recovery_data(args.input)

        # Calculate statistics
        stats_initial = calculate_statistics(initial_times)
        stats_recovery = calculate_statistics(total_recovery_times)

        # Print statistics to console
        print_statistics(stats_initial, stats_recovery, node_count, crash_time)

        # Save summary CSV
        save_summary_csv(output_dir, stats_initial, stats_recovery, node_count, crash_time)

        # Generate plots if requested
        if not args.no_plots:
            # Timeline plot
            timeline_file = output_dir / f'recovery_timeline_{node_count}nodes.png'
            plot_recovery_timeline(trials, initial_times, recovery_times, total_recovery_times,
                                   crash_time, stats_initial, stats_recovery,
                                   node_count, timeline_file, args.dpi)

            # Distribution plot
            dist_file = output_dir / f'recovery_distribution_{node_count}nodes.png'
            plot_recovery_distribution(trials, total_recovery_times, stats_recovery,
                                       node_count, crash_time, dist_file, args.dpi)

        print(f"\nSuccess! Analyzed {stats_initial['count']} trials.")
        print(f"Valid recovery measurements: {stats_recovery['valid_count']}/{stats_recovery['count']}")

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
