#!/usr/bin/env python3
"""
Bully Algorithm Experiment Automation Script

This script automates the execution of Cooja simulations for the Bully
leader election algorithm and collects metrics for analysis.

Usage:
    python3 run_experiment.py --simulation ../bully-cooja.csc --duration 300 --output results/baseline
    python3 run_experiment.py --help
"""

import argparse
import subprocess
import os
import sys
import time
import re
from datetime import datetime
from pathlib import Path


class CoojaExperiment:
    """Manages Cooja simulation execution and log collection"""

    def __init__(self, contiki_path, simulation_file, duration_seconds, output_dir):
        """
        Initialize experiment

        Args:
            contiki_path: Path to Contiki-NG root directory
            simulation_file: Path to .csc simulation file
            duration_seconds: How long to run simulation (in simulation seconds)
            output_dir: Directory to save output files
        """
        self.contiki_path = Path(contiki_path)
        self.simulation_file = Path(simulation_file)
        self.duration_ms = duration_seconds * 1000  # Convert to milliseconds
        self.output_dir = Path(output_dir)

        # Create output directory
        self.output_dir.mkdir(parents=True, exist_ok=True)

        # Output files
        self.log_file = self.output_dir / "cooja_output.log"
        self.metrics_file = self.output_dir / "metrics.csv"
        self.summary_file = self.output_dir / "summary.txt"

    def run(self):
        """Execute the Cooja simulation"""
        print(f"[INFO] Starting Cooja simulation: {self.simulation_file}")
        print(f"[INFO] Duration: {self.duration_ms/1000:.1f}s")
        print(f"[INFO] Output directory: {self.output_dir}")

        # Build the Cooja command for headless execution
        # Try both possible JAR locations (newer Gradle builds use build/libs/)
        cooja_jar = self.contiki_path / "tools" / "cooja" / "build" / "libs" / "cooja.jar"
        if not cooja_jar.exists():
            cooja_jar = self.contiki_path / "tools" / "cooja" / "dist" / "cooja.jar"

        if not cooja_jar.exists():
            print(f"[ERROR] Cooja JAR not found")
            print(f"[ERROR] Checked: tools/cooja/build/libs/cooja.jar and tools/cooja/dist/cooja.jar")
            print("[ERROR] Please build Cooja first: cd tools/cooja && ./gradlew jar")
            return False

        # Command to run Cooja headless (NOGUI mode)
        cmd = [
            "java",
            "--enable-preview",  # Required for Cooja preview features
            "-jar", str(cooja_jar),
            "--no-gui",
            "--autostart",
            "--contiki=" + str(self.contiki_path.resolve()),
            str(self.simulation_file.resolve())
        ]

        print(f"[INFO] Running: {' '.join(cmd)}")
        print(f"[INFO] Simulation will run for {self.duration_ms}ms...")

        # Run Cooja and capture output
        try:
            start_time = time.time()

            # Set environment variable for simulation duration
            env = os.environ.copy()
            env['COOJA_TIMEOUT'] = str(self.duration_ms)

            with open(self.log_file, 'w') as log:
                # Run Cooja process
                process = subprocess.Popen(
                    cmd,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    universal_newlines=True,
                    bufsize=1,
                    env=env
                )

                # Monitor output and write to log file
                for line in process.stdout:
                    log.write(line)
                    log.flush()

                    # Print progress indicators
                    if "Simulation ended" in line or "SHUTDOWN" in line:
                        print("[INFO] Simulation completed")
                        break
                    elif line.strip():
                        # Print selective output
                        if any(keyword in line for keyword in ["METRICS", "ERROR", "WARNING"]):
                            print(f"  {line.strip()}")

                # Wait for process to complete
                process.wait(timeout=self.duration_ms/1000 + 60)  # Add 60s buffer

            elapsed = time.time() - start_time
            print(f"[INFO] Simulation completed in {elapsed:.1f}s")
            print(f"[INFO] Log saved to: {self.log_file}")

            return True

        except subprocess.TimeoutExpired:
            print("[ERROR] Simulation timed out!")
            process.kill()
            return False
        except Exception as e:
            print(f"[ERROR] Simulation failed: {e}")
            return False

    def parse_metrics(self):
        """Parse metrics from Cooja log file"""
        print(f"[INFO] Parsing metrics from {self.log_file}")

        if not self.log_file.exists():
            print("[ERROR] Log file not found!")
            return False

        metrics_lines = []
        header_written = False

        with open(self.log_file, 'r') as log:
            for line in log:
                # Look for METRICS_HEADER and METRICS lines
                if "METRICS_HEADER" in line:
                    # Extract CSV header (only write once)
                    if not header_written:
                        parts = line.split("METRICS_HEADER,")
                        if len(parts) > 1:
                            metrics_lines.append(parts[1].strip())
                            header_written = True
                elif "METRICS," in line and not "METRICS_HEADER" in line:
                    # Extract CSV data
                    parts = line.split("METRICS,")
                    if len(parts) > 1:
                        metrics_lines.append(parts[1].strip())

        # Write metrics to CSV file
        with open(self.metrics_file, 'w') as f:
            for line in metrics_lines:
                f.write(line + '\n')

        print(f"[INFO] Extracted {len(metrics_lines)} metrics entries")
        print(f"[INFO] Metrics saved to: {self.metrics_file}")

        return len(metrics_lines) > 0

    def generate_summary(self):
        """Generate summary statistics from metrics"""
        print(f"[INFO] Generating summary statistics")

        if not self.metrics_file.exists():
            print("[WARNING] Metrics file not found, skipping summary")
            return

        # Read metrics file
        import csv

        with open(self.metrics_file, 'r') as f:
            reader = csv.DictReader(f)
            rows = list(reader)

        if not rows:
            print("[WARNING] No metrics data found")
            return

        # Calculate summary statistics
        num_nodes = len(set(row['node_id'] for row in rows))

        # Aggregate metrics across all nodes
        total_elections = sum(int(row['elections_started']) for row in rows if row['elections_started'])
        total_messages = sum(
            int(row['msg_election_sent']) +
            int(row['msg_answer_sent']) +
            int(row['msg_coordinator_sent']) +
            int(row['msg_alive_sent'])
            for row in rows if row['msg_election_sent']
        )

        # Find final coordinator
        last_row_per_node = {}
        for row in rows:
            node_id = row['node_id']
            last_row_per_node[node_id] = row

        final_leaders = set(row['current_leader'] for row in last_row_per_node.values())

        # Write summary
        with open(self.summary_file, 'w') as f:
            f.write("=" * 60 + "\n")
            f.write("BULLY ALGORITHM EXPERIMENT SUMMARY\n")
            f.write("=" * 60 + "\n\n")
            f.write(f"Experiment Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"Simulation File: {self.simulation_file}\n")
            f.write(f"Duration: {self.duration_ms/1000:.1f}s\n\n")

            f.write("NETWORK CONFIGURATION\n")
            f.write("-" * 60 + "\n")
            f.write(f"Number of Nodes: {num_nodes}\n\n")

            f.write("AGGREGATE METRICS\n")
            f.write("-" * 60 + "\n")
            f.write(f"Total Elections Started: {total_elections}\n")
            f.write(f"Total Messages Sent: {total_messages}\n")
            f.write(f"Final Leader(s): {', '.join(final_leaders)}\n\n")

            f.write("PER-NODE METRICS (Final State)\n")
            f.write("-" * 60 + "\n")
            for node_id in sorted(last_row_per_node.keys(), key=int):
                row = last_row_per_node[node_id]
                f.write(f"\nNode {node_id}:\n")
                f.write(f"  Elections Started: {row['elections_started']}\n")
                f.write(f"  Elections Won: {row['elections_won']}\n")
                f.write(f"  Elections Lost: {row['elections_lost']}\n")
                f.write(f"  Messages Sent: {int(row['msg_election_sent']) + int(row['msg_answer_sent']) + int(row['msg_coordinator_sent']) + int(row['msg_alive_sent'])}\n")
                f.write(f"  Messages Received: {int(row['msg_election_recv']) + int(row['msg_answer_recv']) + int(row['msg_coordinator_recv']) + int(row['msg_alive_recv'])}\n")
                f.write(f"  Leader Changes: {row['leader_changes']}\n")
                f.write(f"  Current Leader: {row['current_leader']}\n")

        print(f"[INFO] Summary saved to: {self.summary_file}")

        # Print summary to console
        print("\n" + "=" * 60)
        print("EXPERIMENT SUMMARY")
        print("=" * 60)
        print(f"Nodes: {num_nodes}")
        print(f"Total Elections: {total_elections}")
        print(f"Total Messages: {total_messages}")
        print(f"Final Leader(s): {', '.join(final_leaders)}")
        print("=" * 60 + "\n")


def main():
    parser = argparse.ArgumentParser(
        description="Run Bully algorithm experiment in Cooja",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Run 5-minute baseline experiment
  python3 run_experiment.py --simulation ../bully-cooja.csc --duration 300 --output results/baseline_run1

  # Run 10-minute experiment with custom Contiki path
  python3 run_experiment.py --simulation ../bully-cooja.csc --duration 600 --output results/test1 --contiki /path/to/contiki-ng
        """
    )

    parser.add_argument(
        '--simulation', '-s',
        required=True,
        help='Path to Cooja simulation file (.csc)'
    )

    parser.add_argument(
        '--duration', '-d',
        type=int,
        default=300,
        help='Simulation duration in seconds (default: 300)'
    )

    parser.add_argument(
        '--output', '-o',
        required=True,
        help='Output directory for results'
    )

    parser.add_argument(
        '--contiki', '-c',
        default=None,
        help='Path to Contiki-NG root directory (default: auto-detect)'
    )

    args = parser.parse_args()

    # Auto-detect Contiki-NG path if not provided
    if args.contiki is None:
        # Try to find contiki-ng by going up from current script location
        script_dir = Path(__file__).resolve().parent
        # Assume script is in examples/bully/scripts/
        contiki_path = script_dir.parent.parent.parent
    else:
        contiki_path = Path(args.contiki)

    if not contiki_path.exists():
        print(f"[ERROR] Contiki-NG path not found: {contiki_path}")
        print("[ERROR] Please specify --contiki /path/to/contiki-ng")
        return 1

    print(f"[INFO] Using Contiki-NG at: {contiki_path}")

    # Create and run experiment
    experiment = CoojaExperiment(
        contiki_path=contiki_path,
        simulation_file=args.simulation,
        duration_seconds=args.duration,
        output_dir=args.output
    )

    # Run simulation
    if not experiment.run():
        print("[ERROR] Simulation failed!")
        return 1

    # Parse metrics
    if not experiment.parse_metrics():
        print("[WARNING] Metrics parsing failed or no metrics found")

    # Generate summary
    experiment.generate_summary()

    print("\n[SUCCESS] Experiment completed successfully!")
    print(f"[SUCCESS] Results saved to: {experiment.output_dir}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
