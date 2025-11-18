#!/usr/bin/env python3
"""
Bully Algorithm Results Visualization

This script creates visualizations from Bully algorithm metrics.

Usage:
    python3 plot_results.py metrics.csv --output plots/
    python3 plot_results.py metrics.csv --type convergence --output plots/convergence.png

Requirements:
    pip install matplotlib pandas
"""

import argparse
import sys
from pathlib import Path

try:
    import pandas as pd
    import matplotlib.pyplot as plt
    import matplotlib
    matplotlib.use('Agg')  # Use non-interactive backend
except ImportError:
    print("[ERROR] Required packages not found!")
    print("[ERROR] Please install: pip install matplotlib pandas")
    sys.exit(1)


class ResultsVisualizer:
    """Creates visualizations from Bully algorithm metrics"""

    def __init__(self, metrics_file):
        """Initialize visualizer with metrics CSV file"""
        self.metrics_file = Path(metrics_file)
        self.df = pd.read_csv(metrics_file)
        self.nodes = self.df['node_id'].unique()

        print(f"Loaded {len(self.df)} metrics entries for {len(self.nodes)} nodes")

    def plot_convergence_time(self, output_file):
        """Plot convergence time per node"""
        # Get first convergence time for each node
        convergence_data = self.df[self.df['first_convergence_time'] > 0].groupby('node_id')['first_convergence_time'].first()

        if convergence_data.empty:
            print("[WARNING] No convergence data available")
            return

        fig, ax = plt.subplots(figsize=(10, 6))
        convergence_data.plot(kind='bar', ax=ax, color='steelblue')
        ax.set_xlabel('Node ID')
        ax.set_ylabel('Convergence Time (ticks)')
        ax.set_title('Time to First Leader Election (Convergence Time)')
        ax.grid(axis='y', alpha=0.3)
        plt.tight_layout()
        plt.savefig(output_file, dpi=300)
        print(f"Saved convergence plot to {output_file}")
        plt.close()

    def plot_message_overhead(self, output_file):
        """Plot message overhead per node"""
        # Get final message counts for each node
        final_state = self.df.groupby('node_id').last()

        sent_data = {
            'ELECTION': final_state['msg_election_sent'],
            'ANSWER': final_state['msg_answer_sent'],
            'COORDINATOR': final_state['msg_coordinator_sent'],
            'ALIVE': final_state['msg_alive_sent']
        }

        df_sent = pd.DataFrame(sent_data, index=final_state.index)

        fig, ax = plt.subplots(figsize=(12, 6))
        df_sent.plot(kind='bar', stacked=True, ax=ax)
        ax.set_xlabel('Node ID')
        ax.set_ylabel('Messages Sent')
        ax.set_title('Message Overhead per Node (by Type)')
        ax.legend(title='Message Type')
        ax.grid(axis='y', alpha=0.3)
        plt.tight_layout()
        plt.savefig(output_file, dpi=300)
        print(f"Saved message overhead plot to {output_file}")
        plt.close()

    def plot_election_statistics(self, output_file):
        """Plot election statistics"""
        final_state = self.df.groupby('node_id').last()

        election_data = pd.DataFrame({
            'Started': final_state['elections_started'],
            'Won': final_state['elections_won'],
            'Lost': final_state['elections_lost']
        }, index=final_state.index)

        fig, ax = plt.subplots(figsize=(10, 6))
        election_data.plot(kind='bar', ax=ax)
        ax.set_xlabel('Node ID')
        ax.set_ylabel('Election Count')
        ax.set_title('Election Statistics per Node')
        ax.legend()
        ax.grid(axis='y', alpha=0.3)
        plt.tight_layout()
        plt.savefig(output_file, dpi=300)
        print(f"Saved election statistics plot to {output_file}")
        plt.close()

    def plot_state_distribution(self, output_file):
        """Plot time spent in each state"""
        final_state = self.df.groupby('node_id').last()

        state_data = pd.DataFrame({
            'NORMAL': final_state['time_in_normal'],
            'ELECTION': final_state['time_in_election'],
            'WAITING': final_state['time_in_waiting']
        }, index=final_state.index)

        fig, ax = plt.subplots(figsize=(12, 6))
        state_data.plot(kind='bar', stacked=True, ax=ax, color=['green', 'orange', 'red'])
        ax.set_xlabel('Node ID')
        ax.set_ylabel('Time (ticks)')
        ax.set_title('State Distribution per Node')
        ax.legend(title='State')
        ax.grid(axis='y', alpha=0.3)
        plt.tight_layout()
        plt.savefig(output_file, dpi=300)
        print(f"Saved state distribution plot to {output_file}")
        plt.close()

    def plot_leader_timeline(self, output_file):
        """Plot leader changes over time"""
        # For each timestamp, show which node is the leader from each node's perspective
        # This helps visualize convergence and leader stability

        fig, ax = plt.subplots(figsize=(14, 6))

        for node_id in sorted(self.nodes):
            node_data = self.df[self.df['node_id'] == node_id]
            ax.plot(node_data['timestamp'], node_data['current_leader'],
                   marker='o', label=f'Node {node_id}', alpha=0.7)

        ax.set_xlabel('Time (ticks)')
        ax.set_ylabel('Current Leader ID')
        ax.set_title('Leader Convergence Timeline (All Nodes)')
        ax.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
        ax.grid(True, alpha=0.3)
        plt.tight_layout()
        plt.savefig(output_file, dpi=300)
        print(f"Saved leader timeline plot to {output_file}")
        plt.close()

    def plot_message_timeline(self, output_file):
        """Plot cumulative messages over time"""
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 10))

        # Plot 1: Total messages sent over time
        for node_id in sorted(self.nodes):
            node_data = self.df[self.df['node_id'] == node_id]
            total_sent = (node_data['msg_election_sent'] +
                         node_data['msg_answer_sent'] +
                         node_data['msg_coordinator_sent'] +
                         node_data['msg_alive_sent'])
            ax1.plot(node_data['timestamp'], total_sent, label=f'Node {node_id}', marker='o')

        ax1.set_xlabel('Time (ticks)')
        ax1.set_ylabel('Total Messages Sent')
        ax1.set_title('Message Transmission Timeline')
        ax1.legend()
        ax1.grid(True, alpha=0.3)

        # Plot 2: Election messages only
        for node_id in sorted(self.nodes):
            node_data = self.df[self.df['node_id'] == node_id]
            ax2.plot(node_data['timestamp'], node_data['elections_started'],
                    label=f'Node {node_id}', marker='s')

        ax2.set_xlabel('Time (ticks)')
        ax2.set_ylabel('Elections Started')
        ax2.set_title('Election Activity Timeline')
        ax2.legend()
        ax2.grid(True, alpha=0.3)

        plt.tight_layout()
        plt.savefig(output_file, dpi=300)
        print(f"Saved message timeline plot to {output_file}")
        plt.close()

    def create_all_plots(self, output_dir):
        """Create all available plots"""
        output_dir = Path(output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)

        self.plot_convergence_time(output_dir / 'convergence.png')
        self.plot_message_overhead(output_dir / 'message_overhead.png')
        self.plot_election_statistics(output_dir / 'elections.png')
        self.plot_state_distribution(output_dir / 'state_distribution.png')
        self.plot_leader_timeline(output_dir / 'leader_timeline.png')
        self.plot_message_timeline(output_dir / 'message_timeline.png')

        print(f"\nAll plots saved to {output_dir}/")


def main():
    parser = argparse.ArgumentParser(
        description="Visualize Bully algorithm metrics",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument(
        'metrics_file',
        help='Path to metrics CSV file'
    )

    parser.add_argument(
        '--output', '-o',
        required=True,
        help='Output file or directory for plots'
    )

    parser.add_argument(
        '--type', '-t',
        choices=['convergence', 'messages', 'elections', 'states', 'timeline', 'all'],
        default='all',
        help='Type of plot to generate (default: all)'
    )

    args = parser.parse_args()

    try:
        visualizer = ResultsVisualizer(args.metrics_file)
        output_path = Path(args.output)

        if args.type == 'all':
            visualizer.create_all_plots(output_path)
        elif args.type == 'convergence':
            visualizer.plot_convergence_time(output_path)
        elif args.type == 'messages':
            visualizer.plot_message_overhead(output_path)
        elif args.type == 'elections':
            visualizer.plot_election_statistics(output_path)
        elif args.type == 'states':
            visualizer.plot_state_distribution(output_path)
        elif args.type == 'timeline':
            visualizer.plot_leader_timeline(output_path)

        print("\n[SUCCESS] Visualization complete!")

    except Exception as e:
        print(f"[ERROR] {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
