#!/usr/bin/env python3
"""
Leader Election Algorithm Metrics Parser and Analyzer

This script parses CSV metrics from leader election algorithm experiments and
computes aggregate statistics for analysis. Supports all algorithms:
bully, ring, prasle, adaptive-prasle.

Features:
  - Basic statistics (mean, std, min, max, median)
  - Confidence intervals (95% CI)
  - Statistical significance tests (t-test, Mann-Whitney U)
  - Effect size calculations (Cohen's d)
  - Multi-trial analysis

Usage:
    python3 parse_metrics.py metrics.csv --output analysis/
    python3 parse_metrics.py metrics.csv --summary
    python3 parse_metrics.py metrics.csv --algorithm bully --summary
    python3 parse_metrics.py --compare results/bully results/ring --output comparison/

Examples:
    # Basic analysis of single trial
    python3 parse_metrics.py trial_1/metrics.csv --summary

    # Compare two algorithms with statistical tests
    python3 parse_metrics.py --compare results/bully results/ring --stat-test

    # Multi-trial analysis with confidence intervals
    python3 parse_metrics.py --multi-trial results/bully/convergence_trials/
"""

import argparse
import csv
import sys
import math
from pathlib import Path
from collections import defaultdict
import statistics
from typing import List, Dict, Optional, Tuple

# Optional scipy for advanced statistical tests
try:
    from scipy import stats as scipy_stats
    HAS_SCIPY = True
except ImportError:
    HAS_SCIPY = False


class MetricsAnalyzer:
    """Analyzes leader election algorithm metrics"""

    def __init__(self, metrics_file, algorithm='bully'):
        """Initialize analyzer with metrics CSV file"""
        self.metrics_file = Path(metrics_file)
        self.algorithm = algorithm
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
                if row.get('first_convergence_time') and int(row['first_convergence_time']) > 0:
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
            'by_type': {}
        }

        for node_id in self.nodes:
            node_data = [row for row in self.data if row['node_id'] == node_id]
            if not node_data:
                continue
            last_row = node_data[-1]  # Get final state

            # Algorithm-specific message type handling
            if self.algorithm == 'bully':
                sent = (int(last_row.get('msg_election_sent', 0)) +
                       int(last_row.get('msg_answer_sent', 0)) +
                       int(last_row.get('msg_coordinator_sent', 0)) +
                       int(last_row.get('msg_alive_sent', 0)))

                recv = (int(last_row.get('msg_election_recv', 0)) +
                       int(last_row.get('msg_answer_recv', 0)) +
                       int(last_row.get('msg_coordinator_recv', 0)) +
                       int(last_row.get('msg_alive_recv', 0)))

                overhead['by_type']['election'] = overhead['by_type'].get('election', 0) + int(last_row.get('msg_election_sent', 0))
                overhead['by_type']['answer'] = overhead['by_type'].get('answer', 0) + int(last_row.get('msg_answer_sent', 0))
                overhead['by_type']['coordinator'] = overhead['by_type'].get('coordinator', 0) + int(last_row.get('msg_coordinator_sent', 0))
                overhead['by_type']['alive'] = overhead['by_type'].get('alive', 0) + int(last_row.get('msg_alive_sent', 0))
            else:
                # Generic message counting for ring, prasle, adaptive-prasle
                sent = int(last_row.get('messages_sent', 0))
                recv = int(last_row.get('messages_received', 0))

            overhead['per_node'][node_id] = {
                'sent': sent,
                'received': recv
            }

            overhead['total_sent'] += sent
            overhead['total_received'] += recv

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
            if not node_data:
                continue
            last_row = node_data[-1]

            started = int(last_row.get('elections_started', 0))
            won = int(last_row.get('elections_won', 0))
            lost = int(last_row.get('elections_lost', 0))

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
            if not node_data:
                continue
            last_row = node_data[-1]

            changes = int(last_row.get('leader_changes', 0))
            stability['per_node'][node_id] = {
                'leader_changes': changes,
                'final_leader': last_row.get('current_leader', 'unknown')
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
            if not node_data:
                continue
            last_row = node_data[-1]

            time_normal = int(last_row.get('time_in_normal', 0))
            time_election = int(last_row.get('time_in_election', 0))
            time_waiting = int(last_row.get('time_in_waiting', 0))
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
        algo_name = self.algorithm.upper()
        print("\n" + "=" * 70)
        print(f" {algo_name} ALGORITHM METRICS ANALYSIS")
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
        if self.nodes:
            print(f"  Avg messages per node: {overhead['total_sent'] / len(self.nodes):.1f}")
        if overhead['by_type']:
            print(f"\n  By message type:")
            for msg_type, count in overhead['by_type'].items():
                print(f"    {msg_type.upper():12s}: {count}")

        # Election Statistics
        elections = self.compute_election_statistics()
        print("\n3. ELECTION STATISTICS")
        print("-" * 70)
        print(f"  Total elections started: {elections['total_elections']}")
        print(f"  Total elections won: {elections['total_won']}")
        print(f"  Total elections lost: {elections['total_lost']}")
        if self.nodes:
            print(f"  Avg elections per node: {elections['total_elections'] / len(self.nodes):.1f}")

        # Leader Stability
        stability = self.compute_leader_stability()
        print("\n4. LEADER STABILITY")
        print("-" * 70)
        print(f"  Total leader changes: {stability['total_leader_changes']}")
        if 'avg_leader_changes' in stability:
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


# =============================================================================
# Statistical Analysis Functions
# =============================================================================

class StatisticalAnalyzer:
    """
    Advanced statistical analysis for leader election experiments.

    Provides:
      - Confidence intervals (95% CI)
      - Statistical significance tests (t-test, Mann-Whitney U)
      - Effect size calculations (Cohen's d)
    """

    # T-distribution critical values for 95% CI (two-tailed)
    # Key: degrees of freedom, Value: t-critical
    T_CRITICAL = {
        1: 12.706, 2: 4.303, 3: 3.182, 4: 2.776, 5: 2.571,
        6: 2.447, 7: 2.365, 8: 2.306, 9: 2.262, 10: 2.228,
        11: 2.201, 12: 2.179, 13: 2.160, 14: 2.145, 15: 2.131,
        16: 2.120, 17: 2.110, 18: 2.101, 19: 2.093, 20: 2.086,
        25: 2.060, 30: 2.042, 40: 2.021, 50: 2.009, 60: 2.000,
        80: 1.990, 100: 1.984, 120: 1.980
    }

    @staticmethod
    def get_t_critical(df: int) -> float:
        """Get t-critical value for given degrees of freedom."""
        if df in StatisticalAnalyzer.T_CRITICAL:
            return StatisticalAnalyzer.T_CRITICAL[df]
        # Interpolate or use closest value
        keys = sorted(StatisticalAnalyzer.T_CRITICAL.keys())
        if df < keys[0]:
            return StatisticalAnalyzer.T_CRITICAL[keys[0]]
        if df > keys[-1]:
            return 1.96  # Approximate for large df (z-distribution)
        # Find closest
        for i, k in enumerate(keys):
            if k > df:
                return StatisticalAnalyzer.T_CRITICAL[keys[i-1]]
        return 1.96

    @staticmethod
    def confidence_interval(data: List[float], confidence: float = 0.95) -> Tuple[float, float, float]:
        """
        Calculate confidence interval for a dataset.

        Args:
            data: List of numeric values
            confidence: Confidence level (default 0.95 for 95% CI)

        Returns:
            Tuple of (mean, lower_bound, upper_bound)
        """
        n = len(data)
        if n < 2:
            mean = data[0] if data else 0
            return (mean, mean, mean)

        mean = statistics.mean(data)
        std = statistics.stdev(data)
        se = std / math.sqrt(n)  # Standard error

        # Use scipy if available for exact t-value
        if HAS_SCIPY:
            t_value = scipy_stats.t.ppf((1 + confidence) / 2, n - 1)
        else:
            t_value = StatisticalAnalyzer.get_t_critical(n - 1)

        margin = t_value * se
        return (mean, mean - margin, mean + margin)

    @staticmethod
    def cohens_d(group1: List[float], group2: List[float]) -> float:
        """
        Calculate Cohen's d effect size between two groups.

        Interpretation:
            |d| < 0.2: negligible
            0.2 <= |d| < 0.5: small
            0.5 <= |d| < 0.8: medium
            |d| >= 0.8: large

        Args:
            group1: First group data
            group2: Second group data

        Returns:
            Cohen's d effect size
        """
        n1, n2 = len(group1), len(group2)
        if n1 < 2 or n2 < 2:
            return 0.0

        mean1, mean2 = statistics.mean(group1), statistics.mean(group2)
        var1, var2 = statistics.variance(group1), statistics.variance(group2)

        # Pooled standard deviation
        pooled_std = math.sqrt(((n1 - 1) * var1 + (n2 - 1) * var2) / (n1 + n2 - 2))

        if pooled_std == 0:
            return 0.0

        return (mean1 - mean2) / pooled_std

    @staticmethod
    def interpret_cohens_d(d: float) -> str:
        """Interpret Cohen's d effect size."""
        abs_d = abs(d)
        if abs_d < 0.2:
            return "negligible"
        elif abs_d < 0.5:
            return "small"
        elif abs_d < 0.8:
            return "medium"
        else:
            return "large"

    @staticmethod
    def t_test(group1: List[float], group2: List[float], paired: bool = False) -> Dict:
        """
        Perform independent or paired t-test.

        Args:
            group1: First group data
            group2: Second group data
            paired: Whether to perform paired t-test

        Returns:
            Dict with t-statistic, p-value, significance
        """
        if not HAS_SCIPY:
            return {
                'error': 'scipy not installed',
                't_statistic': None,
                'p_value': None,
                'significant': None
            }

        n1, n2 = len(group1), len(group2)
        if n1 < 2 or n2 < 2:
            return {
                'error': 'Insufficient data',
                't_statistic': None,
                'p_value': None,
                'significant': None
            }

        if paired:
            if n1 != n2:
                return {
                    'error': 'Paired t-test requires equal group sizes',
                    't_statistic': None,
                    'p_value': None,
                    'significant': None
                }
            t_stat, p_value = scipy_stats.ttest_rel(group1, group2)
        else:
            t_stat, p_value = scipy_stats.ttest_ind(group1, group2)

        return {
            't_statistic': t_stat,
            'p_value': p_value,
            'significant': p_value < 0.05,
            'highly_significant': p_value < 0.01
        }

    @staticmethod
    def mann_whitney_u(group1: List[float], group2: List[float]) -> Dict:
        """
        Perform Mann-Whitney U test (non-parametric alternative to t-test).

        Use when data may not be normally distributed.

        Args:
            group1: First group data
            group2: Second group data

        Returns:
            Dict with U-statistic, p-value, significance
        """
        if not HAS_SCIPY:
            return {
                'error': 'scipy not installed',
                'u_statistic': None,
                'p_value': None,
                'significant': None
            }

        n1, n2 = len(group1), len(group2)
        if n1 < 2 or n2 < 2:
            return {
                'error': 'Insufficient data',
                'u_statistic': None,
                'p_value': None,
                'significant': None
            }

        u_stat, p_value = scipy_stats.mannwhitneyu(group1, group2, alternative='two-sided')

        return {
            'u_statistic': u_stat,
            'p_value': p_value,
            'significant': p_value < 0.05,
            'highly_significant': p_value < 0.01
        }

    @staticmethod
    def descriptive_stats(data: List[float]) -> Dict:
        """
        Calculate comprehensive descriptive statistics.

        Args:
            data: List of numeric values

        Returns:
            Dict with all descriptive statistics
        """
        if not data:
            return {
                'n': 0, 'mean': 0, 'std': 0, 'se': 0,
                'min': 0, 'max': 0, 'median': 0,
                'ci_lower': 0, 'ci_upper': 0
            }

        n = len(data)
        mean_val = statistics.mean(data)
        std_val = statistics.stdev(data) if n > 1 else 0
        se_val = std_val / math.sqrt(n) if n > 0 else 0

        mean, ci_lower, ci_upper = StatisticalAnalyzer.confidence_interval(data)

        return {
            'n': n,
            'mean': mean_val,
            'std': std_val,
            'se': se_val,
            'min': min(data),
            'max': max(data),
            'median': statistics.median(data),
            'ci_lower': ci_lower,
            'ci_upper': ci_upper,
            'ci_margin': (ci_upper - ci_lower) / 2
        }


class MultiTrialAnalyzer:
    """Analyze results across multiple trials for statistical robustness."""

    def __init__(self, results_dir: Path, algorithm: str = 'bully'):
        """
        Initialize multi-trial analyzer.

        Args:
            results_dir: Directory containing trial subdirectories
            algorithm: Algorithm name
        """
        self.results_dir = Path(results_dir)
        self.algorithm = algorithm
        self.trials = []
        self._load_trials()

    def _load_trials(self):
        """Load data from all trial directories."""
        if not self.results_dir.exists():
            print(f"Warning: Results directory not found: {self.results_dir}")
            return

        # Look for trial directories
        trial_dirs = sorted([
            d for d in self.results_dir.iterdir()
            if d.is_dir() and d.name.startswith('trial_')
        ])

        # Also check for convergence_trials subdirectory
        conv_trials = self.results_dir / 'convergence_trials'
        if conv_trials.exists():
            for subdir in conv_trials.iterdir():
                if subdir.is_dir():
                    trial_dirs.extend([
                        d for d in subdir.iterdir()
                        if d.is_dir() and d.name.startswith('trial_')
                    ])

        for trial_dir in trial_dirs:
            metrics_file = trial_dir / 'metrics.csv'
            if metrics_file.exists():
                try:
                    analyzer = MetricsAnalyzer(metrics_file, self.algorithm)
                    self.trials.append({
                        'name': trial_dir.name,
                        'path': trial_dir,
                        'analyzer': analyzer
                    })
                except Exception as e:
                    print(f"Warning: Could not load {trial_dir}: {e}")

        print(f"Loaded {len(self.trials)} trials from {self.results_dir}")

    def get_convergence_times(self) -> List[float]:
        """Get convergence times across all trials."""
        times = []
        for trial in self.trials:
            convergence = trial['analyzer'].compute_convergence_time()
            if convergence:
                # Use the maximum convergence time (when all nodes agree)
                times.append(max(convergence.values()))
        return times

    def get_message_counts(self) -> List[int]:
        """Get total message counts across all trials."""
        counts = []
        for trial in self.trials:
            overhead = trial['analyzer'].compute_message_overhead()
            counts.append(overhead['total_sent'])
        return counts

    def get_election_counts(self) -> List[int]:
        """Get election counts across all trials."""
        counts = []
        for trial in self.trials:
            elections = trial['analyzer'].compute_election_statistics()
            counts.append(elections['total_elections'])
        return counts

    def analyze(self) -> Dict:
        """
        Perform comprehensive statistical analysis across all trials.

        Returns:
            Dict with statistics for all metrics
        """
        convergence_times = self.get_convergence_times()
        message_counts = self.get_message_counts()
        election_counts = self.get_election_counts()

        return {
            'algorithm': self.algorithm,
            'num_trials': len(self.trials),
            'convergence_time': StatisticalAnalyzer.descriptive_stats(convergence_times),
            'message_count': StatisticalAnalyzer.descriptive_stats([float(x) for x in message_counts]),
            'election_count': StatisticalAnalyzer.descriptive_stats([float(x) for x in election_counts])
        }

    def print_summary(self):
        """Print statistical summary to console."""
        analysis = self.analyze()

        print("\n" + "=" * 70)
        print(f" MULTI-TRIAL STATISTICAL ANALYSIS: {self.algorithm.upper()}")
        print("=" * 70)
        print(f"  Number of trials: {analysis['num_trials']}")

        if analysis['num_trials'] < 2:
            print("\n  WARNING: Need at least 2 trials for statistical analysis")
            return

        # Convergence Time
        ct = analysis['convergence_time']
        print("\n  CONVERGENCE TIME (ticks)")
        print("-" * 70)
        print(f"    Mean:   {ct['mean']:.1f} ± {ct['std']:.1f}")
        print(f"    95% CI: [{ct['ci_lower']:.1f}, {ct['ci_upper']:.1f}]")
        print(f"    Range:  [{ct['min']:.1f}, {ct['max']:.1f}]")
        print(f"    Median: {ct['median']:.1f}")

        # Message Count
        mc = analysis['message_count']
        print("\n  MESSAGE COUNT")
        print("-" * 70)
        print(f"    Mean:   {mc['mean']:.1f} ± {mc['std']:.1f}")
        print(f"    95% CI: [{mc['ci_lower']:.1f}, {mc['ci_upper']:.1f}]")
        print(f"    Range:  [{mc['min']:.0f}, {mc['max']:.0f}]")

        # Election Count
        ec = analysis['election_count']
        print("\n  ELECTION COUNT")
        print("-" * 70)
        print(f"    Mean:   {ec['mean']:.1f} ± {ec['std']:.1f}")
        print(f"    95% CI: [{ec['ci_lower']:.1f}, {ec['ci_upper']:.1f}]")
        print(f"    Range:  [{ec['min']:.0f}, {ec['max']:.0f}]")

        print("\n" + "=" * 70)


def compare_algorithms(algo1_dir: Path, algo2_dir: Path, algo1_name: str = 'algo1',
                       algo2_name: str = 'algo2', output_dir: Optional[Path] = None):
    """
    Compare two algorithms with statistical tests.

    Args:
        algo1_dir: Results directory for first algorithm
        algo2_dir: Results directory for second algorithm
        algo1_name: Name of first algorithm
        algo2_name: Name of second algorithm
        output_dir: Optional output directory for results
    """
    print("\n" + "=" * 70)
    print(f" STATISTICAL COMPARISON: {algo1_name.upper()} vs {algo2_name.upper()}")
    print("=" * 70)

    # Load multi-trial data
    analyzer1 = MultiTrialAnalyzer(algo1_dir, algo1_name)
    analyzer2 = MultiTrialAnalyzer(algo2_dir, algo2_name)

    # Get data
    conv1 = analyzer1.get_convergence_times()
    conv2 = analyzer2.get_convergence_times()
    msg1 = [float(x) for x in analyzer1.get_message_counts()]
    msg2 = [float(x) for x in analyzer2.get_message_counts()]

    results = []

    # Convergence Time Comparison
    print("\n1. CONVERGENCE TIME COMPARISON")
    print("-" * 70)

    stats1 = StatisticalAnalyzer.descriptive_stats(conv1)
    stats2 = StatisticalAnalyzer.descriptive_stats(conv2)

    print(f"  {algo1_name}: {stats1['mean']:.1f} ± {stats1['std']:.1f} (n={stats1['n']})")
    print(f"  {algo2_name}: {stats2['mean']:.1f} ± {stats2['std']:.1f} (n={stats2['n']})")

    if len(conv1) >= 2 and len(conv2) >= 2:
        # T-test
        t_result = StatisticalAnalyzer.t_test(conv1, conv2)
        if 'error' not in t_result or t_result['error'] is None:
            print(f"\n  T-test: t={t_result['t_statistic']:.3f}, p={t_result['p_value']:.4f}")
            print(f"  Significant difference: {'Yes' if t_result['significant'] else 'No'} (α=0.05)")

        # Mann-Whitney U
        mw_result = StatisticalAnalyzer.mann_whitney_u(conv1, conv2)
        if 'error' not in mw_result or mw_result['error'] is None:
            print(f"\n  Mann-Whitney U: U={mw_result['u_statistic']:.1f}, p={mw_result['p_value']:.4f}")

        # Effect size
        d = StatisticalAnalyzer.cohens_d(conv1, conv2)
        print(f"\n  Cohen's d: {d:.3f} ({StatisticalAnalyzer.interpret_cohens_d(d)} effect)")

        results.append({
            'metric': 'convergence_time',
            'algo1_mean': stats1['mean'],
            'algo1_std': stats1['std'],
            'algo2_mean': stats2['mean'],
            'algo2_std': stats2['std'],
            'cohens_d': d,
            'effect_size': StatisticalAnalyzer.interpret_cohens_d(d),
            't_statistic': t_result.get('t_statistic'),
            'p_value': t_result.get('p_value'),
            'significant': t_result.get('significant')
        })

    # Message Count Comparison
    print("\n2. MESSAGE COUNT COMPARISON")
    print("-" * 70)

    stats1 = StatisticalAnalyzer.descriptive_stats(msg1)
    stats2 = StatisticalAnalyzer.descriptive_stats(msg2)

    print(f"  {algo1_name}: {stats1['mean']:.1f} ± {stats1['std']:.1f} (n={stats1['n']})")
    print(f"  {algo2_name}: {stats2['mean']:.1f} ± {stats2['std']:.1f} (n={stats2['n']})")

    if len(msg1) >= 2 and len(msg2) >= 2:
        t_result = StatisticalAnalyzer.t_test(msg1, msg2)
        if 'error' not in t_result or t_result['error'] is None:
            print(f"\n  T-test: t={t_result['t_statistic']:.3f}, p={t_result['p_value']:.4f}")
            print(f"  Significant difference: {'Yes' if t_result['significant'] else 'No'} (α=0.05)")

        d = StatisticalAnalyzer.cohens_d(msg1, msg2)
        print(f"\n  Cohen's d: {d:.3f} ({StatisticalAnalyzer.interpret_cohens_d(d)} effect)")

        results.append({
            'metric': 'message_count',
            'algo1_mean': stats1['mean'],
            'algo1_std': stats1['std'],
            'algo2_mean': stats2['mean'],
            'algo2_std': stats2['std'],
            'cohens_d': d,
            'effect_size': StatisticalAnalyzer.interpret_cohens_d(d),
            't_statistic': t_result.get('t_statistic'),
            'p_value': t_result.get('p_value'),
            'significant': t_result.get('significant')
        })

    # Export if requested
    if output_dir and results:
        output_dir = Path(output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)

        csv_file = output_dir / f'comparison_{algo1_name}_vs_{algo2_name}.csv'
        with open(csv_file, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=results[0].keys())
            writer.writeheader()
            writer.writerows(results)
        print(f"\n  Results exported to: {csv_file}")

    print("\n" + "=" * 70)

    if not HAS_SCIPY:
        print("\n  Note: Install scipy for more accurate statistical tests:")
        print("        pip install scipy")


def main():
    parser = argparse.ArgumentParser(
        description="Parse and analyze leader election algorithm metrics",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  Basic analysis:
    python3 parse_metrics.py metrics.csv --summary

  Multi-trial analysis with confidence intervals:
    python3 parse_metrics.py --multi-trial results/bully/ -a bully

  Compare two algorithms with statistical tests:
    python3 parse_metrics.py --compare results/bully results/ring -o comparison/

Statistical Features:
  - 95% Confidence Intervals (CI) for all metrics
  - Student's t-test for comparing algorithms
  - Mann-Whitney U test (non-parametric alternative)
  - Cohen's d effect size with interpretation

Note: Install scipy for advanced statistical tests:
  pip install scipy
        """
    )

    parser.add_argument(
        'metrics_file',
        nargs='?',
        help='Path to metrics CSV file (for single-trial analysis)'
    )

    parser.add_argument(
        '--algorithm', '-a',
        choices=['bully', 'ring', 'prasle', 'adaptive-prasle'],
        default='bully',
        help='Algorithm type (default: bully)'
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

    parser.add_argument(
        '--multi-trial', '-m',
        metavar='DIR',
        help='Analyze multiple trials from a directory (computes 95%% CI)'
    )

    parser.add_argument(
        '--compare', '-c',
        nargs=2,
        metavar=('ALGO1_DIR', 'ALGO2_DIR'),
        help='Compare two algorithms with statistical tests'
    )

    parser.add_argument(
        '--algo-names',
        nargs=2,
        metavar=('NAME1', 'NAME2'),
        default=None,
        help='Names for algorithms when comparing (default: inferred from directory names)'
    )

    parser.add_argument(
        '--stat-test',
        action='store_true',
        help='Include detailed statistical tests in output'
    )

    args = parser.parse_args()

    try:
        # Mode 1: Compare two algorithms
        if args.compare:
            algo1_dir, algo2_dir = args.compare

            # Infer algorithm names from directory paths if not provided
            if args.algo_names:
                algo1_name, algo2_name = args.algo_names
            else:
                algo1_name = Path(algo1_dir).name
                algo2_name = Path(algo2_dir).name

            compare_algorithms(
                Path(algo1_dir), Path(algo2_dir),
                algo1_name, algo2_name,
                Path(args.output) if args.output else None
            )
            return 0

        # Mode 2: Multi-trial analysis
        if args.multi_trial:
            analyzer = MultiTrialAnalyzer(Path(args.multi_trial), args.algorithm)
            analyzer.print_summary()

            # Export if output specified
            if args.output:
                output_dir = Path(args.output)
                output_dir.mkdir(parents=True, exist_ok=True)

                analysis = analyzer.analyze()
                csv_file = output_dir / f'{args.algorithm}_multi_trial_stats.csv'

                with open(csv_file, 'w', newline='') as f:
                    writer = csv.writer(f)
                    writer.writerow(['metric', 'n', 'mean', 'std', 'se', 'ci_lower', 'ci_upper', 'min', 'max', 'median'])

                    for metric_name in ['convergence_time', 'message_count', 'election_count']:
                        stats = analysis[metric_name]
                        writer.writerow([
                            metric_name,
                            stats['n'],
                            f"{stats['mean']:.2f}",
                            f"{stats['std']:.2f}",
                            f"{stats['se']:.2f}",
                            f"{stats['ci_lower']:.2f}",
                            f"{stats['ci_upper']:.2f}",
                            f"{stats['min']:.2f}",
                            f"{stats['max']:.2f}",
                            f"{stats['median']:.2f}"
                        ])

                print(f"\nStatistical analysis exported to: {csv_file}")

            return 0

        # Mode 3: Single-file analysis (original behavior)
        if not args.metrics_file:
            parser.print_help()
            print("\nError: Must provide metrics_file, --multi-trial, or --compare")
            return 1

        analyzer = MetricsAnalyzer(args.metrics_file, args.algorithm)

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
