#!/usr/bin/env python3
"""
Bully Algorithm Metrics Parser and Analyzer

This script parses CSV metrics from Bully algorithm experiments and
computes aggregate statistics for analysis.

Usage:
    python3 parse_metrics.py metrics.csv --output analysis/
    python3 parse_metrics.py metrics.csv --summary
"""

import argparse
import csv
import sys
from pathlib import Path
from collections import defaultdict
import statistics


class MetricsAnalyzer:
    """Analyzes Bully algorithm metrics"""

    def __init__(self, metrics_file):
        """Initialize analyzer with metrics CSV file"""
        self.metrics_file = Path(metrics_file)
        self.data = []
        self.nodes = set()

        self._load_data()

    def _load_data(self):
        """Load metrics from CSV file"""
        if not self.metrics_file.exists():
            raise FileNotFoundError(f"Metrics file not found: {self.metrics_file}")

        with open(self.metrics_file, 'r') as f:
            reader = csv.DictReader(f)
            self.data = list(reader)

        # Extract unique node IDs
        self.nodes = set(row['node_id'] for row in self.data)

        print(f"Loaded {len(self.data)} metrics entries for {len(self.nodes)} nodes")

    def compute_convergence_time(self):
        """
        Compute convergence time (time to first stable leader)

        Returns:
            dict: Convergence time per node in simulation ticks
        """
        convergence = {}

        for node_id in self.nodes:
            node_data = [row for row in self.data if row['node_id'] == node_id]

            for row in node_data:
                if row['first_convergence_time'] and int(row['first_convergence_time']) > 0:
                    convergence[node_id] = int(row['first_convergence_time'])
                    break

        return convergence

    def compute_message_overhead(self):
        """
        Compute total message overhead

        Returns:
            dict: Message counts per node and totals
        """
        overhead = {
            'per_node': {},
            'total_sent': 0,
            'total_received': 0,
            'by_type': {
                'election': 0,
                'answer': 0,
                'coordinator': 0,
                'alive': 0
            }
        }

        for node_id in self.nodes:
            node_data = [row for row in self.data if row['node_id'] == node_id]
            last_row = node_data[-1]  # Get final state

            sent = (int(last_row['msg_election_sent']) +
                   int(last_row['msg_answer_sent']) +
                   int(last_row['msg_coordinator_sent']) +
                   int(last_row['msg_alive_sent']))

            recv = (int(last_row['msg_election_recv']) +
                   int(last_row['msg_answer_recv']) +
                   int(last_row['msg_coordinator_recv']) +
                   int(last_row['msg_alive_recv']))

            overhead['per_node'][node_id] = {
                'sent': sent,
                'received': recv
            }

            overhead['total_sent'] += sent
            overhead['total_received'] += recv

            overhead['by_type']['election'] += int(last_row['msg_election_sent'])
            overhead['by_type']['answer'] += int(last_row['msg_answer_sent'])
            overhead['by_type']['coordinator'] += int(last_row['msg_coordinator_sent'])
            overhead['by_type']['alive'] += int(last_row['msg_alive_sent'])

        return overhead

    def compute_election_statistics(self):
        """
        Compute election-related statistics

        Returns:
            dict: Election statistics
        """
        stats = {
            'total_elections': 0,
            'per_node': {},
            'total_won': 0,
            'total_lost': 0,
        }

        for node_id in self.nodes:
            node_data = [row for row in self.data if row['node_id'] == node_id]
            last_row = node_data[-1]

            started = int(last_row['elections_started'])
            won = int(last_row['elections_won'])
            lost = int(last_row['elections_lost'])

            stats['per_node'][node_id] = {
                'started': started,
                'won': won,
                'lost': lost
            }

            stats['total_elections'] += started
            stats['total_won'] += won
            stats['total_lost'] += lost

        return stats

    def compute_leader_stability(self):
        """
        Compute leader stability metrics

        Returns:
            dict: Leader changes and stability metrics
        """
        stability = {
            'total_leader_changes': 0,
            'per_node': {},
            'leader_history': []
        }

        for node_id in self.nodes:
            node_data = [row for row in self.data if row['node_id'] == node_id]
            last_row = node_data[-1]

            changes = int(last_row['leader_changes'])
            stability['per_node'][node_id] = {
                'leader_changes': changes,
                'final_leader': last_row['current_leader']
            }

            stability['total_leader_changes'] += changes

        # Calculate average leader changes
        if self.nodes:
            stability['avg_leader_changes'] = stability['total_leader_changes'] / len(self.nodes)

        return stability

    def compute_state_distribution(self):
        """
        Compute time spent in each state

        Returns:
            dict: State distribution metrics
        """
        distribution = {
            'per_node': {},
            'aggregate': {
                'normal': 0,
                'election': 0,
                'waiting': 0
            }
        }

        for node_id in self.nodes:
            node_data = [row for row in self.data if row['node_id'] == node_id]
            last_row = node_data[-1]

            time_normal = int(last_row['time_in_normal'])
            time_election = int(last_row['time_in_election'])
            time_waiting = int(last_row['time_in_waiting'])
            total = time_normal + time_election + time_waiting

            distribution['per_node'][node_id] = {
                'normal': time_normal,
                'election': time_election,
                'waiting': time_waiting,
                'total': total,
                'normal_pct': (time_normal / total * 100) if total > 0 else 0,
                'election_pct': (time_election / total * 100) if total > 0 else 0,
                'waiting_pct': (time_waiting / total * 100) if total > 0 else 0
            }

            distribution['aggregate']['normal'] += time_normal
            distribution['aggregate']['election'] += time_election
            distribution['aggregate']['waiting'] += time_waiting

        return distribution

    def generate_summary(self):
        """Generate comprehensive summary of metrics"""
        print("\n" + "=" * 70)
        print(" BULLY ALGORITHM METRICS ANALYSIS")
        print("=" * 70)

        # Convergence Time
        convergence = self.compute_convergence_time()
        print("\n1. CONVERGENCE TIME")
        print("-" * 70)
        if convergence:
            conv_times = list(convergence.values())
            print(f"  Min convergence time: {min(conv_times)} ticks")
            print(f"  Max convergence time: {max(conv_times)} ticks")
            print(f"  Avg convergence time: {statistics.mean(conv_times):.1f} ticks")
        else:
            print("  No convergence data available")

        # Message Overhead
        overhead = self.compute_message_overhead()
        print("\n2. MESSAGE OVERHEAD")
        print("-" * 70)
        print(f"  Total messages sent: {overhead['total_sent']}")
        print(f"  Total messages received: {overhead['total_received']}")
        print(f"  Avg messages per node: {overhead['total_sent'] / len(self.nodes):.1f}")
        print(f"\n  By message type:")
        print(f"    ELECTION:    {overhead['by_type']['election']}")
        print(f"    ANSWER:      {overhead['by_type']['answer']}")
        print(f"    COORDINATOR: {overhead['by_type']['coordinator']}")
        print(f"    ALIVE:       {overhead['by_type']['alive']}")

        # Election Statistics
        elections = self.compute_election_statistics()
        print("\n3. ELECTION STATISTICS")
        print("-" * 70)
        print(f"  Total elections started: {elections['total_elections']}")
        print(f"  Total elections won: {elections['total_won']}")
        print(f"  Total elections lost: {elections['total_lost']}")
        print(f"  Avg elections per node: {elections['total_elections'] / len(self.nodes):.1f}")

        # Leader Stability
        stability = self.compute_leader_stability()
        print("\n4. LEADER STABILITY")
        print("-" * 70)
        print(f"  Total leader changes: {stability['total_leader_changes']}")
        print(f"  Avg leader changes per node: {stability['avg_leader_changes']:.1f}")

        # Find final leader consensus
        final_leaders = set(node['final_leader'] for node in stability['per_node'].values())
        if len(final_leaders) == 1:
            print(f"  Final leader (consensus): Node {final_leaders.pop()}")
        else:
            print(f"  WARNING: No consensus! Final leaders: {final_leaders}")

        # State Distribution
        distribution = self.compute_state_distribution()
        print("\n5. STATE DISTRIBUTION (Aggregate)")
        print("-" * 70)
        total_time = sum(distribution['aggregate'].values())
        if total_time > 0:
            print(f"  Time in NORMAL:  {distribution['aggregate']['normal']:8d} ticks ({distribution['aggregate']['normal']/total_time*100:5.1f}%)")
            print(f"  Time in ELECTION: {distribution['aggregate']['election']:8d} ticks ({distribution['aggregate']['election']/total_time*100:5.1f}%)")
            print(f"  Time in WAITING:  {distribution['aggregate']['waiting']:8d} ticks ({distribution['aggregate']['waiting']/total_time*100:5.1f}%)")

        print("\n" + "=" * 70 + "\n")

    def export_analysis(self, output_dir):
        """Export detailed analysis to files"""
        output_dir = Path(output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)

        # Convergence
        convergence = self.compute_convergence_time()
        with open(output_dir / 'convergence.csv', 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['node_id', 'convergence_time_ticks'])
            for node_id, time in sorted(convergence.items(), key=lambda x: int(x[0])):
                writer.writerow([node_id, time])

        # Message overhead
        overhead = self.compute_message_overhead()
        with open(output_dir / 'message_overhead.csv', 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['node_id', 'messages_sent', 'messages_received'])
            for node_id in sorted(overhead['per_node'].keys(), key=int):
                node_data = overhead['per_node'][node_id]
                writer.writerow([node_id, node_data['sent'], node_data['received']])

        # Elections
        elections = self.compute_election_statistics()
        with open(output_dir / 'elections.csv', 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['node_id', 'elections_started', 'elections_won', 'elections_lost'])
            for node_id in sorted(elections['per_node'].keys(), key=int):
                node_data = elections['per_node'][node_id]
                writer.writerow([node_id, node_data['started'], node_data['won'], node_data['lost']])

        print(f"Analysis exported to {output_dir}/")


def main():
    parser = argparse.ArgumentParser(
        description="Parse and analyze Bully algorithm metrics",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument(
        'metrics_file',
        help='Path to metrics CSV file'
    )

    parser.add_argument(
        '--output', '-o',
        help='Output directory for analysis files'
    )

    parser.add_argument(
        '--summary', '-s',
        action='store_true',
        help='Print summary to console'
    )

    args = parser.parse_args()

    try:
        analyzer = MetricsAnalyzer(args.metrics_file)

        if args.summary or not args.output:
            analyzer.generate_summary()

        if args.output:
            analyzer.export_analysis(args.output)

    except Exception as e:
        print(f"[ERROR] {e}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
