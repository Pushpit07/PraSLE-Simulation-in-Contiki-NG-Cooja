#!/usr/bin/env python3
"""
Bully Algorithm Convergence Time Distribution Plotter

This script visualizes the distribution of convergence times from multiple trials,
showing individual measurements, mean, and standard deviation.

Usage:
    python3 plot_convergence_distribution.py -i <csv_file> -o <output_png>

Parameters:
    -i, --input    Path to convergence_times.csv file (required)
    -o, --output   Output PNG file path (default: convergence_distribution.png)
    --title        Custom plot title (optional)
    --dpi          DPI for output image (default: 300)
"""

import argparse
import csv
import statistics
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


def load_convergence_data(csv_file):
    """
    Load convergence time data from CSV file.

    Args:
        csv_file: Path to CSV file with columns: trial, node_count, convergence_time_ms

    Returns:
        tuple: (trials, node_count, convergence_times)
    """
    trials = []
    convergence_times = []
    node_count = None

    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            if row['convergence_time_ms'] != 'NA':
                trial_num = int(row['trial'])
                time_ms = float(row['convergence_time_ms'])
                node_count = int(row['node_count'])

                trials.append(trial_num)
                convergence_times.append(time_ms)

    if not convergence_times:
        raise ValueError("No valid convergence times found in CSV file")

    return trials, node_count, convergence_times


def calculate_statistics(data):
    """
    Calculate statistical measures for the data.

    Args:
        data: List of numerical values

    Returns:
        dict: Dictionary containing statistical measures
    """
    return {
        'mean': statistics.mean(data),
        'median': statistics.median(data),
        'stdev': statistics.stdev(data) if len(data) > 1 else 0,
        'min': min(data),
        'max': max(data),
        'range': max(data) - min(data),
        'count': len(data),
        'cv': (statistics.stdev(data) / statistics.mean(data) * 100) if len(data) > 1 else 0
    }


def print_statistics(stats, node_count):
    """
    Print formatted statistics table to console.

    Args:
        stats: Dictionary of statistical measures
        node_count: Number of nodes in the experiment
    """
    print("\nConvergence Time Statistics:")
    print("=" * 60)
    print(f"Trials:        {stats['count']}")
    print(f"Node Count:    {node_count}")
    print(f"Mean:          {stats['mean']:.2f} ms")
    print(f"Median:        {stats['median']:.2f} ms")
    print(f"Std Dev:       {stats['stdev']:.2f} ms")
    print(f"Min:           {stats['min']:.2f} ms")
    print(f"Max:           {stats['max']:.2f} ms")
    print(f"Range:         {stats['range']:.2f} ms")
    print(f"CV:            {stats['cv']:.2f}%")
    print("=" * 60)
    print()


def plot_convergence_distribution(trials, convergence_times, stats, node_count,
                                   output_file, title=None, dpi=300):
    """
    Create and save convergence time distribution plot.

    Args:
        trials: List of trial numbers
        convergence_times: List of convergence times in ms
        stats: Dictionary of statistical measures
        node_count: Number of nodes
        output_file: Path to save the PNG file
        title: Custom title (optional)
        dpi: DPI for output image
    """
    # Set up the figure and axis
    plt.figure(figsize=(12, 7))
    ax = plt.gca()

    # Plot individual measurements as scatter points
    plt.scatter(trials, convergence_times, alpha=0.6, s=30,
                color='steelblue', label='Individual measurements', zorder=3)

    # Plot mean line
    mean_val = stats['mean']
    plt.axhline(y=mean_val, color='red', linestyle='--', linewidth=2,
                label=f'Mean: {mean_val:.1f} ms', zorder=4)

    # Plot ±1σ shaded region
    std_val = stats['stdev']
    plt.axhspan(mean_val - std_val, mean_val + std_val,
                alpha=0.2, color='gray', label=f'±1σ: {std_val:.1f} ms', zorder=1)

    # Configure plot
    if title:
        plt.title(title, fontsize=14, fontweight='bold', pad=20)
    else:
        plt.title(f'Convergence Time Distribution - {node_count} Nodes ({stats["count"]} trials)',
                  fontsize=14, fontweight='bold', pad=20)

    plt.xlabel('Trial Number', fontsize=12)
    plt.ylabel('Convergence Time (ms)', fontsize=12)
    plt.grid(True, alpha=0.3, linestyle=':', zorder=0)
    plt.legend(loc='best', fontsize=10, framealpha=0.9)

    # Set axis limits with some padding
    y_min = min(convergence_times)
    y_max = max(convergence_times)
    y_padding = (y_max - y_min) * 0.1
    plt.ylim(y_min - y_padding, y_max + y_padding)

    x_padding = max(trials) * 0.02
    plt.xlim(0 - x_padding, max(trials) + x_padding)

    # Add text box with statistics
    textstr = '\n'.join((
        f'Mean = {mean_val:.1f} ms',
        f'Median = {stats["median"]:.1f} ms',
        f'Std Dev = {std_val:.1f} ms',
        f'Min = {stats["min"]:.1f} ms',
        f'Max = {stats["max"]:.1f} ms',
        f'CV = {stats["cv"]:.1f}%'
    ))
    props = dict(boxstyle='round', facecolor='wheat', alpha=0.8)
    ax.text(0.02, 0.98, textstr, transform=ax.transAxes, fontsize=9,
            verticalalignment='top', bbox=props, family='monospace')

    # Tight layout
    plt.tight_layout()

    # Save figure
    plt.savefig(output_file, dpi=dpi, bbox_inches='tight')
    print(f"Plot saved to: {output_file}")

    # Close to free memory
    plt.close()


def main():
    """Main function to parse arguments and generate plot."""
    parser = argparse.ArgumentParser(
        description='Plot convergence time distribution from trial data',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s -i results/convergence_trials/50nodes_20251201/convergence_times.csv
  %(prog)s -i data.csv -o output.png --title "Custom Title"
  %(prog)s -i data.csv --dpi 150
        """
    )

    parser.add_argument('-i', '--input', required=True,
                        help='Path to convergence_times.csv file')
    parser.add_argument('-o', '--output', default='convergence_distribution.png',
                        help='Output PNG file path (default: convergence_distribution.png)')
    parser.add_argument('--title', default=None,
                        help='Custom plot title')
    parser.add_argument('--dpi', type=int, default=300,
                        help='DPI for output image (default: 300)')

    args = parser.parse_args()

    # Check if input file exists
    if not Path(args.input).exists():
        print(f"Error: Input file not found: {args.input}", file=sys.stderr)
        sys.exit(1)

    try:
        # Load data
        print(f"Loading data from: {args.input}")
        trials, node_count, convergence_times = load_convergence_data(args.input)

        # Calculate statistics
        stats = calculate_statistics(convergence_times)

        # Print statistics to console
        print_statistics(stats, node_count)

        # Generate and save plot
        plot_convergence_distribution(trials, convergence_times, stats, node_count,
                                       args.output, args.title, args.dpi)

        print(f"\nSuccess! Processed {stats['count']} trials.")

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
