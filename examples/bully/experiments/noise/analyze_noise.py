#!/usr/bin/env python3
"""
Bully Algorithm Network Disruption Analysis

This script analyzes and visualizes network disruption (noise) experiment data,
showing convergence time and message overhead under various noise levels.

Usage:
    python3 analyze_disruption.py -i <csv_file> [-o <output_dir>]

Parameters:
    -i, --input      Path to noise_convergence_times.csv file (required)
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


def load_noise_data(csv_file):
    """
    Load noise experiment data from CSV file.

    Args:
        csv_file: Path to CSV file with columns:
                  trial, node_count, noise_level, convergence_time_ms, message_count

    Returns:
        tuple: (trials, node_count, noise_level, convergence_times, message_counts)
    """
    trials = []
    convergence_times = []
    message_counts = []
    node_count = None
    noise_level = None

    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            trial_num = int(row['trial'])
            node_count = int(row['node_count'])
            noise_level = int(row['noise_level'])

            trials.append(trial_num)

            # Convergence time
            if row['convergence_time_ms'] != 'NA':
                convergence_times.append(float(row['convergence_time_ms']))
            else:
                convergence_times.append(None)

            # Message count
            if row['message_count'] != 'NA':
                message_counts.append(float(row['message_count']))
            else:
                message_counts.append(None)

    if not trials:
        raise ValueError("No trial data found in CSV file")

    return trials, node_count, noise_level, convergence_times, message_counts


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


def print_statistics(conv_stats, msg_stats, node_count, noise_level):
    """
    Print formatted statistics table to console.

    Args:
        conv_stats: Dictionary of convergence time statistics
        msg_stats: Dictionary of message count statistics
        node_count: Number of nodes in the experiment
        noise_level: Success rate percentage
    """
    print("\nNetwork Disruption Analysis:")
    print("=" * 70)
    print(f"Node Count:        {node_count}")
    print(f"Noise Level:       {noise_level}% success rate ({100-noise_level}% packet loss)")
    print(f"Total Trials:      {conv_stats['count']}")
    print("=" * 70)
    print()

    print("Convergence Time (under noise):")
    print("-" * 70)
    if conv_stats['valid_count'] > 0:
        print(f"  Valid Trials:    {conv_stats['valid_count']}/{conv_stats['count']}")
        print(f"  Mean:            {conv_stats['mean']:.2f} ms")
        print(f"  Median:          {conv_stats['median']:.2f} ms")
        print(f"  Std Dev:         {conv_stats['stdev']:.2f} ms")
        print(f"  Min:             {conv_stats['min']:.2f} ms")
        print(f"  Max:             {conv_stats['max']:.2f} ms")
        print(f"  Range:           {conv_stats['range']:.2f} ms")
        print(f"  CV:              {conv_stats['cv']:.2f}%")
    else:
        print("  No valid data")
    print()

    print("Message Overhead:")
    print("-" * 70)
    if msg_stats['valid_count'] > 0:
        print(f"  Valid Trials:    {msg_stats['valid_count']}/{msg_stats['count']}")
        print(f"  Mean:            {msg_stats['mean']:.0f} messages")
        print(f"  Median:          {msg_stats['median']:.0f} messages")
        print(f"  Std Dev:         {msg_stats['stdev']:.0f} messages")
        print(f"  Min:             {msg_stats['min']:.0f} messages")
        print(f"  Max:             {msg_stats['max']:.0f} messages")
        print()
        if noise_level < 100:
            expected_increase = (100.0 / noise_level - 1) * 100
            print(f"  Expected overhead increase: ~{expected_increase:.0f}%")
            print(f"  (due to {100-noise_level}% packet loss requiring retransmissions)")
    else:
        print("  No valid data")
    print()

    print("=" * 70)


def plot_convergence_distribution(trials, convergence_times, conv_stats,
                                   node_count, noise_level, output_file, dpi=300):
    """
    Create convergence time distribution plot under noise.

    Args:
        trials: List of trial numbers
        convergence_times: List of convergence times (ms)
        conv_stats: Statistics for convergence time
        node_count: Number of nodes
        noise_level: Success rate percentage
        output_file: Path to save the PNG file
        dpi: DPI for output image
    """
    # Filter valid data
    valid_trials = []
    valid_times = []

    for i, trial in enumerate(trials):
        if convergence_times[i] is not None:
            valid_trials.append(trial)
            valid_times.append(convergence_times[i])

    if not valid_trials:
        print("Warning: No valid convergence data to plot")
        return

    plt.figure(figsize=(12, 7))
    ax = plt.gca()

    # Plot individual measurements
    plt.scatter(valid_trials, valid_times, alpha=0.6, s=30,
                color='steelblue', label='Convergence Time', zorder=3)

    # Plot mean line
    mean_val = conv_stats['mean']
    plt.axhline(y=mean_val, color='red', linestyle='--', linewidth=2,
                label=f'Mean: {mean_val:.1f}ms', zorder=4)

    # Plot ±1σ shaded region
    std_val = conv_stats['stdev']
    plt.axhspan(mean_val - std_val, mean_val + std_val,
                alpha=0.2, color='gray', label=f'±1σ: {std_val:.1f}ms', zorder=1)

    # Configure plot
    plt.title(f'Convergence Time Distribution - {node_count} Nodes ({noise_level}% Success Rate)',
              fontsize=14, fontweight='bold', pad=20)
    plt.xlabel('Trial Number', fontsize=12)
    plt.ylabel('Convergence Time (ms)', fontsize=12)
    plt.grid(True, alpha=0.3, linestyle=':', zorder=0)
    plt.legend(loc='best', fontsize=10, framealpha=0.9)

    # Set axis limits with padding
    y_min = min(valid_times)
    y_max = max(valid_times)
    y_padding = (y_max - y_min) * 0.1
    plt.ylim(y_min - y_padding, y_max + y_padding)

    x_padding = max(valid_trials) * 0.02
    plt.xlim(0 - x_padding, max(valid_trials) + x_padding)

    # Add statistics text box
    textstr = '\n'.join((
        f'Convergence Statistics:',
        f'Mean = {mean_val:.1f}ms',
        f'Median = {conv_stats["median"]:.1f}ms',
        f'Std Dev = {std_val:.1f}ms',
        f'Min = {conv_stats["min"]:.1f}ms',
        f'Max = {conv_stats["max"]:.1f}ms',
        f'CV = {conv_stats["cv"]:.1f}%',
        f'',
        f'Packet Loss = {100-noise_level}%'
    ))
    props = dict(boxstyle='round', facecolor='wheat', alpha=0.8)
    ax.text(0.02, 0.98, textstr, transform=ax.transAxes, fontsize=9,
            verticalalignment='top', bbox=props, family='monospace')

    plt.tight_layout()
    plt.savefig(output_file, dpi=dpi, bbox_inches='tight')
    print(f"Convergence distribution plot saved to: {output_file}")
    plt.close()


def plot_message_overhead(trials, message_counts, msg_stats,
                           node_count, noise_level, output_file, dpi=300):
    """
    Create message overhead distribution plot.

    Args:
        trials: List of trial numbers
        message_counts: List of message counts
        msg_stats: Statistics for message counts
        node_count: Number of nodes
        noise_level: Success rate percentage
        output_file: Path to save the PNG file
        dpi: DPI for output image
    """
    # Filter valid data
    valid_trials = []
    valid_counts = []

    for i, trial in enumerate(trials):
        if message_counts[i] is not None:
            valid_trials.append(trial)
            valid_counts.append(message_counts[i])

    if not valid_trials:
        print("Warning: No valid message count data to plot")
        return

    plt.figure(figsize=(12, 7))
    ax = plt.gca()

    # Plot individual measurements
    plt.scatter(valid_trials, valid_counts, alpha=0.6, s=30,
                color='darkorange', label='Message Count', zorder=3)

    # Plot mean line
    mean_val = msg_stats['mean']
    plt.axhline(y=mean_val, color='darkred', linestyle='--', linewidth=2,
                label=f'Mean: {mean_val:.0f} messages', zorder=4)

    # Plot ±1σ shaded region
    std_val = msg_stats['stdev']
    plt.axhspan(mean_val - std_val, mean_val + std_val,
                alpha=0.2, color='gray', label=f'±1σ: {std_val:.0f} messages', zorder=1)

    # Configure plot
    plt.title(f'Message Overhead - {node_count} Nodes ({noise_level}% Success Rate)',
              fontsize=14, fontweight='bold', pad=20)
    plt.xlabel('Trial Number', fontsize=12)
    plt.ylabel('Total Messages Sent', fontsize=12)
    plt.grid(True, alpha=0.3, linestyle=':', zorder=0)
    plt.legend(loc='best', fontsize=10, framealpha=0.9)

    # Set axis limits with padding
    y_min = min(valid_counts)
    y_max = max(valid_counts)
    y_padding = (y_max - y_min) * 0.1
    plt.ylim(y_min - y_padding, y_max + y_padding)

    x_padding = max(valid_trials) * 0.02
    plt.xlim(0 - x_padding, max(valid_trials) + x_padding)

    # Add statistics text box
    expected_increase = (100.0 / noise_level - 1) * 100 if noise_level < 100 else 0
    textstr = '\n'.join((
        f'Message Overhead:',
        f'Mean = {mean_val:.0f} msgs',
        f'Median = {msg_stats["median"]:.0f} msgs',
        f'Std Dev = {std_val:.0f} msgs',
        f'Min = {msg_stats["min"]:.0f} msgs',
        f'Max = {msg_stats["max"]:.0f} msgs',
        f'',
        f'Packet Loss = {100-noise_level}%',
        f'Expected Increase = ~{expected_increase:.0f}%'
    ))
    props = dict(boxstyle='round', facecolor='lightyellow', alpha=0.8)
    ax.text(0.02, 0.98, textstr, transform=ax.transAxes, fontsize=9,
            verticalalignment='top', bbox=props, family='monospace')

    plt.tight_layout()
    plt.savefig(output_file, dpi=dpi, bbox_inches='tight')
    print(f"Message overhead plot saved to: {output_file}")
    plt.close()


def save_summary_csv(output_dir, conv_stats, msg_stats, node_count, noise_level):
    """
    Save summary statistics to CSV file.

    Args:
        output_dir: Output directory
        conv_stats: Convergence time statistics
        msg_stats: Message count statistics
        node_count: Number of nodes
        noise_level: Success rate percentage
    """
    summary_file = output_dir / 'noise_summary.csv'

    with open(summary_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['Metric', 'Convergence_Time_ms', 'Message_Count'])
        writer.writerow(['Node_Count', node_count, node_count])
        writer.writerow(['Noise_Level_%', noise_level, noise_level])
        writer.writerow(['Packet_Loss_%', 100-noise_level, 100-noise_level])
        writer.writerow(['Valid_Trials', conv_stats['valid_count'], msg_stats['valid_count']])
        writer.writerow(['Mean', f"{conv_stats['mean']:.2f}", f"{msg_stats['mean']:.0f}"])
        writer.writerow(['Median', f"{conv_stats['median']:.2f}", f"{msg_stats['median']:.0f}"])
        writer.writerow(['Std_Dev', f"{conv_stats['stdev']:.2f}", f"{msg_stats['stdev']:.0f}"])
        writer.writerow(['Min', f"{conv_stats['min']:.2f}", f"{msg_stats['min']:.0f}"])
        writer.writerow(['Max', f"{conv_stats['max']:.2f}", f"{msg_stats['max']:.0f}"])
        writer.writerow(['CV_%', f"{conv_stats['cv']:.2f}", f"{msg_stats['cv']:.2f}"])

    print(f"Summary CSV saved to: {summary_file}")


def main():
    """Main function to parse arguments and generate analysis."""
    parser = argparse.ArgumentParser(
        description='Analyze network disruption (noise) experiment data',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s -i results/network_disruption/50nodes_noise90/noise_convergence_times.csv
  %(prog)s -i data.csv -o analysis_output/
  %(prog)s -i data.csv --no-plots
        """
    )

    parser.add_argument('-i', '--input', required=True,
                        help='Path to noise_convergence_times.csv file')
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
        trials, node_count, noise_level, convergence_times, message_counts = \
            load_noise_data(args.input)

        # Calculate statistics
        conv_stats = calculate_statistics(convergence_times)
        msg_stats = calculate_statistics(message_counts)

        # Print statistics to console
        print_statistics(conv_stats, msg_stats, node_count, noise_level)

        # Save summary CSV
        save_summary_csv(output_dir, conv_stats, msg_stats, node_count, noise_level)

        # Generate plots if requested
        if not args.no_plots:
            # Convergence distribution plot
            conv_file = output_dir / f'noise{noise_level}_convergence_{node_count}nodes.png'
            plot_convergence_distribution(trials, convergence_times, conv_stats,
                                          node_count, noise_level, conv_file, args.dpi)

            # Message overhead plot
            msg_file = output_dir / f'noise{noise_level}_messages_{node_count}nodes.png'
            plot_message_overhead(trials, message_counts, msg_stats,
                                  node_count, noise_level, msg_file, args.dpi)

        print(f"\nSuccess! Analyzed {conv_stats['count']} trials.")
        print(f"Valid measurements: {conv_stats['valid_count']}/{conv_stats['count']}")

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
