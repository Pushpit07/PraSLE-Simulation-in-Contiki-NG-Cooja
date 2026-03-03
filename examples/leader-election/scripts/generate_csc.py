#!/usr/bin/env python3
"""
CSC Template Generator for Leader Election Algorithms

Generates Cooja simulation configuration files for all algorithms, node counts,
and experiment types.

Experiment types:
    convergence       - Standard convergence time measurement
    fault_tolerance   - Leader crash/recovery scenarios
    noise             - Network with packet loss (50%, 70%, 90%)
    network_partition - Two isolated partitions

Usage:
    python3 generate_csc.py --algorithm bully --nodes 10 --output csc_templates/
    python3 generate_csc.py --all --output csc_templates/
    python3 generate_csc.py --experiment noise --noise-level 50 --output csc_templates/
    python3 generate_csc.py --experiment fault_tolerance --output csc_templates/
"""

import argparse
import os
from pathlib import Path
import math

ALGORITHMS = ['bully', 'ring', 'prasle', 'adaptive-prasle']
NODE_COUNTS = [5, 10, 50, 100]
NOISE_LEVELS = [50, 70, 90]  # Success rates in percent
TOPOLOGIES = ['clique', 'ring', 'line', 'mesh']  # For PraSLE algorithm

# Partition configurations: {node_count: (a_range, b_range)}
PARTITION_CONFIG = {
    5: ((1, 2), (3, 5)),
    10: ((1, 5), (6, 10)),
    50: ((1, 25), (26, 50)),
    100: ((1, 50), (51, 100))
}

# Base CSC template (common structure)
CSC_TEMPLATE = '''<?xml version="1.0" encoding="UTF-8"?>
<simconf version="2024090601">
  <simulation>
    <title>{title}</title>
    <randomseed>123456</randomseed>
    <motedelay_us>1000000</motedelay_us>
    <radiomedium>
      org.contikios.cooja.radiomediums.UDGM
      <transmitting_range>{tx_range}</transmitting_range>
      <interference_range>{int_range}</interference_range>
      <success_ratio_tx>{success_tx}</success_ratio_tx>
      <success_ratio_rx>{success_rx}</success_ratio_rx>
    </radiomedium>
    <events>
      <logoutput>40000</logoutput>
    </events>
    <motetype>
      org.contikios.cooja.contikimote.ContikiMoteType
      <identifier>{algorithm}1</identifier>
      <description>{algorithm_upper} Node Type</description>
      <source>[CONTIKI_DIR]/examples/leader-election/{algorithm}-node.c</source>
      <commands>$(MAKE) -j$(CPUS) {algorithm}-node.cooja TARGET=cooja ALGORITHM={algorithm}{topology_flag}{network_size_flag}{fast_mode_flag}</commands>
      <firmware>[CONTIKI_DIR]/examples/leader-election/build/cooja/{algorithm}-node.cooja</firmware>
      <moteinterface>org.contikios.cooja.interfaces.Position</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.Battery</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiVib</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiMoteID</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiRS232</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiBeeper</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.IPAddress</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiRadio</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiButton</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiPIR</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiClock</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiLED</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiCFS</moteinterface>
      <moteinterface>org.contikios.cooja.contikimote.interfaces.ContikiEEPROM</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.Mote2MoteRelations</moteinterface>
      <moteinterface>org.contikios.cooja.interfaces.MoteAttributes</moteinterface>
    </motetype>
{motes}
  </simulation>
  <plugin>
    org.contikios.cooja.plugins.Visualizer
    <plugin_config>
      <moterelations>true</moterelations>
      <skin>org.contikios.cooja.plugins.skins.IDVisualizerSkin</skin>
      <skin>org.contikios.cooja.plugins.skins.GridVisualizerSkin</skin>
      <skin>org.contikios.cooja.plugins.skins.TrafficVisualizerSkin</skin>
      <skin>org.contikios.cooja.plugins.skins.UDGMVisualizerSkin</skin>
      <viewport>0.5 0.0 0.0 0.5 0.0 0.0</viewport>
    </plugin_config>
    <bounds x="1" y="1" height="400" width="400" z="2"/>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.LogListener
    <plugin_config>
      <filter />
      <formatted_time />
      <coloring />
    </plugin_config>
    <bounds x="400" y="1" height="400" width="800" z="1"/>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.TimeLine
    <plugin_config>
      <mote>0</mote>
      <showRadioRXTX />
      <showRadioHW />
      <showLEDs />
      <zoomfactor>500.0</zoomfactor>
    </plugin_config>
    <bounds x="0" y="400" height="200" width="1200" z="0"/>
  </plugin>
  <plugin>
    org.contikios.cooja.plugins.ScriptRunner
    <plugin_config>
      <script>
{script}
      </script>
      <active>true</active>
    </plugin_config>
    <bounds x="800" y="100" height="400" width="600" z="3"/>
  </plugin>
</simconf>
'''

MOTE_TEMPLATE = '''    <mote>
      <interface_config>
        org.contikios.cooja.interfaces.Position
        <x>{x}</x>
        <y>{y}</y>
        <z>0.0</z>
      </interface_config>
      <interface_config>
        org.contikios.cooja.contikimote.interfaces.ContikiMoteID
        <id>{mote_id}</id>
      </interface_config>
      <motetype_identifier>{algorithm}1</motetype_identifier>
    </mote>'''

# Standard script for convergence/noise/partition experiments
STANDARD_SCRIPT = '''/* {algorithm_upper} Leader Election - Headless Execution Script */

// Read duration from environment or use default (5 minutes = 300000ms)
var duration = java.lang.System.getenv("COOJA_TIMEOUT");
if (duration == null) {{
  duration = 300000;
}} else {{
  duration = parseInt(duration);
}}

java.lang.System.out.println("===== SCRIPT STARTED =====");
java.lang.System.out.println("Duration: " + duration + "ms");
java.lang.System.out.println("Motes: " + sim.getMotesCount());

// Schedule simulation end message
GENERATE_MSG(duration, "sim_end");

/* Listen to all mote output and forward to stdout */
var msgCount = 0;
while(true) {{
  YIELD();

  msgCount++;

  // Check for simulation end
  if (msg.equals("sim_end")) {{
    java.lang.System.out.println("===== SIMULATION END =====");
    java.lang.System.out.println("Messages received: " + msgCount);
    log.testOK();
  }}

  // Forward mote output to stdout
  java.lang.System.out.println(time + " " + id + " " + msg);
}}'''

# Crash script for fault tolerance experiments
CRASH_SCRIPT = '''/* {algorithm_upper} Leader Election - Leader Crash Scenario Script */

// Read duration from environment or use default (5 minutes = 300000ms)
var duration = java.lang.System.getenv("COOJA_TIMEOUT");
if (duration == null) {{
  duration = 300000;
}} else {{
  duration = parseInt(duration);
}}

// Read crash time from environment or use default (60 seconds = 60000ms)
var crashTime = java.lang.System.getenv("CRASH_TIME");
if (crashTime == null) {{
  crashTime = 60000;  // Default: crash after 60 seconds
}} else {{
  crashTime = parseInt(crashTime);
}}

java.lang.System.out.println("===== LEADER CRASH SCENARIO STARTED =====");
java.lang.System.out.println("Duration: " + duration + "ms");
java.lang.System.out.println("Crash time: " + crashTime + "ms");
java.lang.System.out.println("Motes: " + sim.getMotesCount());

// Schedule leader crash and simulation end
GENERATE_MSG(crashTime, "crash_leader");
GENERATE_MSG(duration, "sim_end");

var leaderCrashed = false;

/* Listen to all mote output and forward to stdout */
var msgCount = 0;
while(true) {{
  YIELD();

  msgCount++;

  // Check for leader crash event
  if (msg.equals("crash_leader") &amp;&amp; !leaderCrashed) {{
    var moteCount = sim.getMotesCount();
    var leaderMote = sim.getMoteWithID(moteCount);  // Highest ID = leader

    if (leaderMote != null) {{
      java.lang.System.out.println("===== CRASHING LEADER NODE " + moteCount + " at " + time + "ms =====");
      sim.removeMote(leaderMote);
      leaderCrashed = true;
      java.lang.System.out.println("===== LEADER NODE REMOVED - RE-ELECTION SHOULD START =====");
    }} else {{
      java.lang.System.out.println("WARNING: Leader mote not found!");
    }}
  }}

  // Check for simulation end
  if (msg.equals("sim_end")) {{
    java.lang.System.out.println("===== SIMULATION END =====");
    java.lang.System.out.println("Messages received: " + msgCount);
    java.lang.System.out.println("Leader crashed: " + leaderCrashed);
    log.testOK();
  }}

  // Forward mote output to stdout
  java.lang.System.out.println(time + " " + id + " " + msg);
}}'''


def generate_positions_grid(node_count, spacing=30.0):
    """Generate grid positions for nodes."""
    positions = []
    cols = int(math.ceil(math.sqrt(node_count)))
    spacing = max(spacing, 200 / cols)

    for i in range(node_count):
        x = 20 + (i % cols) * spacing
        y = 20 + (i // cols) * spacing
        positions.append((x, y))

    return positions


def generate_positions_ring(node_count):
    """Generate circular positions for ring topology.

    Adjacent nodes in the ring should be within radio range of each other,
    but non-adjacent nodes should NOT be in range (to enforce ring topology).
    """
    positions = []
    # Use larger radius for more nodes to maintain separation
    radius = 50 + node_count * 3

    for i in range(node_count):
        angle = 2 * math.pi * i / node_count
        x = 100 + radius * math.cos(angle)
        y = 100 + radius * math.sin(angle)
        positions.append((x, y))

    return positions


def generate_positions_line(node_count, spacing=40.0):
    """Generate linear positions for line topology.

    Nodes are placed in a horizontal line. Each node can only reach
    its immediate neighbors (left and right).
    """
    positions = []
    for i in range(node_count):
        x = 20 + i * spacing
        y = 100
        positions.append((x, y))
    return positions


def generate_positions_mesh(node_count, spacing=50.0):
    """Generate grid positions for mesh topology.

    Nodes are placed in a 2D grid. Each node can reach its 4-connected
    neighbors (up, down, left, right) but NOT diagonal neighbors.
    """
    positions = []
    cols = int(math.ceil(math.sqrt(node_count)))

    for i in range(node_count):
        x = 20 + (i % cols) * spacing
        y = 20 + (i // cols) * spacing
        positions.append((x, y))

    return positions


def generate_positions_partition(node_count):
    """Generate positions for network partition (two isolated groups).

    The two partitions are placed far enough apart that they cannot communicate
    even with increased radio range for full-mesh connectivity within each partition.
    Separation of 600 units ensures isolation even with ~380 unit radio range.
    """
    positions = []
    partition_a, partition_b = PARTITION_CONFIG[node_count]
    a_count = partition_a[1] - partition_a[0] + 1
    b_count = partition_b[1] - partition_b[0] + 1

    # Position partition A at y=20
    cols_a = int(math.ceil(math.sqrt(a_count)))
    spacing_a = max(22, 150 / cols_a)
    for i in range(a_count):
        x = 20 + (i % cols_a) * spacing_a
        y = 20 + (i // cols_a) * spacing_a
        positions.append((x, y))

    # Position partition B at y=600 (separated by ~500+ units - outside max TX range)
    # This ensures partitions remain isolated even with full-mesh radio range (~380 units)
    cols_b = int(math.ceil(math.sqrt(b_count)))
    spacing_b = max(22, 150 / cols_b)
    for i in range(b_count):
        x = 20 + (i % cols_b) * spacing_b
        y = 600 + (i // cols_b) * spacing_b
        positions.append((x, y))

    return positions


def generate_positions(node_count, algorithm, experiment, topology='clique'):
    """Generate positions based on algorithm, experiment type, and topology."""
    if experiment == 'network_partition':
        return generate_positions_partition(node_count)
    elif algorithm == 'ring':
        return generate_positions_ring(node_count)
    elif algorithm in ['prasle', 'adaptive-prasle'] and topology != 'clique':
        # Use topology-specific positioning for PraSLE
        if topology == 'ring':
            return generate_positions_ring(node_count)
        elif topology == 'line':
            return generate_positions_line(node_count)
        elif topology == 'mesh':
            return generate_positions_mesh(node_count)
    # Default: grid layout for clique (full-mesh) connectivity
    return generate_positions_grid(node_count)


def generate_motes_xml(algorithm, node_count, positions):
    """Generate XML for all motes."""
    motes = []

    for i, (x, y) in enumerate(positions, 1):
        mote_xml = MOTE_TEMPLATE.format(
            x=f"{x:.1f}",
            y=f"{y:.1f}",
            mote_id=i,
            algorithm=algorithm
        )
        motes.append(mote_xml)

    return '\n'.join(motes)


def get_tx_range(algorithm, node_count, experiment, topology='clique'):
    """Calculate appropriate transmission range.

    For clique topology (Bully, PraSLE with clique):
        Radio range must cover the entire grid diagonal for full-mesh connectivity.
        This ensures all nodes can hear each other for correct leader election.

    For ring topology:
        Radio range only needs to reach adjacent nodes in the ring.
        Must NOT reach nodes 2 hops away to enforce ring topology.

    For line topology:
        Radio range only needs to reach immediate neighbors.
        Must NOT reach nodes 2 hops away.

    For mesh topology:
        Radio range reaches 4-connected neighbors (up/down/left/right).
        Must NOT reach diagonal neighbors.
    """
    if algorithm == 'ring' or (algorithm in ['prasle', 'adaptive-prasle'] and topology == 'ring'):
        # Ring: reach adjacent nodes only
        radius = 50 + node_count * 3
        arc_length = 2 * math.pi * radius / node_count
        # Range should be > 1 arc length but < 2 arc lengths
        return arc_length * 1.3
    elif algorithm in ['prasle', 'adaptive-prasle'] and topology == 'line':
        # Line: reach immediate neighbors only (spacing = 40)
        spacing = 40.0
        return spacing * 1.3  # Reach 1 hop but not 2 hops
    elif algorithm in ['prasle', 'adaptive-prasle'] and topology == 'mesh':
        # Mesh: reach 4-connected neighbors but not diagonals (spacing = 50)
        spacing = 50.0
        # diagonal = spacing * sqrt(2) ≈ 70.7, so range should be < 70
        return spacing * 1.2  # ~60, reaches orthogonal but not diagonal
    else:
        # Clique/full-mesh connectivity: radio range must cover entire grid diagonal
        # This ensures all nodes can hear broadcast messages from any other node
        cols = int(math.ceil(math.sqrt(node_count)))
        rows = int(math.ceil(node_count / cols))
        spacing = max(30, 200 / cols)
        grid_width = (cols - 1) * spacing
        grid_height = (rows - 1) * spacing
        diagonal = math.sqrt(grid_width**2 + grid_height**2)
        return diagonal * 1.1  # 10% margin for reliability


def get_success_ratio(experiment, noise_level=None):
    """Get TX/RX success ratios based on experiment type."""
    if experiment == 'noise' and noise_level is not None:
        # noise_level is the success percentage (50, 70, 90)
        # But the naming convention is inverted: noise90 = 10% success
        # Actually looking at the CSC files, noise50 = 0.5 success ratio
        ratio = noise_level / 100.0
        return ratio, ratio
    return 1.0, 1.0


def get_title(algorithm, node_count, experiment, noise_level=None, topology='clique'):
    """Generate appropriate title for the simulation."""
    algo_upper = algorithm.upper().replace('-', '_')
    topo_str = f" ({topology.capitalize()} Topology)" if topology != 'clique' else ""

    if experiment == 'convergence':
        return f"{algo_upper} Leader Election - {node_count} Nodes{topo_str}"
    elif experiment == 'fault_tolerance':
        return f"{algo_upper} Leader Election - {node_count} Nodes{topo_str} - Leader Crash Scenario"
    elif experiment == 'noise':
        return f"{algo_upper} Leader Election - {node_count} Nodes{topo_str} - {noise_level}% Success Rate (Network Noise)"
    elif experiment == 'network_partition':
        return f"{algo_upper} Leader Election - {node_count} Nodes{topo_str} - Network Partition Scenario"
    return f"{algo_upper} Leader Election - {node_count} Nodes{topo_str}"


def get_script(algorithm, experiment):
    """Get appropriate Cooja script based on experiment type."""
    algo_upper = algorithm.upper().replace('-', '_')

    if experiment == 'fault_tolerance':
        return CRASH_SCRIPT.format(algorithm_upper=algo_upper)
    else:
        return STANDARD_SCRIPT.format(algorithm_upper=algo_upper)


def get_output_filename(node_count, experiment, noise_level=None):
    """Generate output filename based on experiment type."""
    if experiment == 'convergence':
        return f'{node_count}nodes.csc'
    elif experiment == 'fault_tolerance':
        return f'{node_count}nodes-crash.csc'
    elif experiment == 'noise':
        return f'{node_count}nodes-noise{noise_level}.csc'
    elif experiment == 'network_partition':
        return f'{node_count}nodes-partition.csc'
    return f'{node_count}nodes.csc'


def generate_csc(algorithm, node_count, output_dir, experiment='convergence',
                 noise_level=None, fast_mode=False, topology='clique'):
    """Generate a single CSC file."""
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    positions = generate_positions(node_count, algorithm, experiment, topology)
    motes_xml = generate_motes_xml(algorithm, node_count, positions)
    tx_range = get_tx_range(algorithm, node_count, experiment, topology)
    success_tx, success_rx = get_success_ratio(experiment, noise_level)
    title = get_title(algorithm, node_count, experiment, noise_level, topology)
    script = get_script(algorithm, experiment)

    # Add build flags
    fast_mode_flag = " FAST_MODE=1" if fast_mode else ""

    # Add topology flag for PraSLE algorithms
    topology_flag = ""
    network_size_flag = ""
    if algorithm in ['prasle', 'adaptive-prasle'] and topology != 'clique':
        topology_flag = f" TOPOLOGY={topology}"
        network_size_flag = f" NETWORK_SIZE={node_count}"

    content = CSC_TEMPLATE.format(
        title=title,
        algorithm=algorithm,
        algorithm_upper=algorithm.upper().replace('-', '_'),
        node_count=node_count,
        motes=motes_xml,
        tx_range=f"{tx_range:.1f}",
        int_range=f"{tx_range:.1f}",
        success_tx=success_tx,
        success_rx=success_rx,
        script=script,
        topology_flag=topology_flag,
        network_size_flag=network_size_flag,
        fast_mode_flag=fast_mode_flag
    )

    output_filename = get_output_filename(node_count, experiment, noise_level)
    output_file = output_dir / output_filename
    output_file.write_text(content)
    print(f'Generated: {output_file}')
    return output_file


def generate_all_for_experiment(output_base, experiment, algorithms=None,
                                 node_counts=None, noise_levels=None, fast_mode=False,
                                 topology='clique'):
    """Generate all CSC files for a specific experiment type."""
    algorithms = algorithms or ALGORITHMS
    node_counts = node_counts or NODE_COUNTS
    noise_levels = noise_levels or NOISE_LEVELS

    generated = 0

    for algo in algorithms:
        for nc in node_counts:
            # Determine output subdirectory based on algorithm and topology
            if algo in ['prasle', 'adaptive-prasle'] and topology != 'clique':
                output_dir = Path(output_base) / f"{algo}-{topology}"
            else:
                output_dir = Path(output_base) / algo

            if experiment == 'noise':
                for nl in noise_levels:
                    generate_csc(algo, nc, output_dir, experiment, nl, fast_mode, topology)
                    generated += 1
            else:
                generate_csc(algo, nc, output_dir, experiment, fast_mode=fast_mode, topology=topology)
                generated += 1

    return generated


def main():
    parser = argparse.ArgumentParser(
        description='Generate CSC templates for leader election algorithms',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --algorithm bully --nodes 10 --output csc_templates/
  %(prog)s --all --output csc_templates/
  %(prog)s --experiment noise --noise-level 50 --output csc_templates/
  %(prog)s --experiment fault_tolerance --output csc_templates/
  %(prog)s --experiment network_partition --all --output csc_templates/

Topology examples (PraSLE only):
  %(prog)s --algorithm prasle --topology ring --output csc_templates/
  %(prog)s --algorithm prasle --topology mesh --nodes 50 --output csc_templates/
  %(prog)s --algorithm prasle --topology line --experiment convergence --output csc_templates/
        """
    )
    parser.add_argument('--algorithm', '-a', choices=ALGORITHMS + ['all'], default='all',
                        help='Algorithm to generate CSC for (default: all)')
    parser.add_argument('--nodes', '-n', type=int, choices=NODE_COUNTS,
                        help='Number of nodes (default: all)')
    parser.add_argument('--output', '-o', required=True,
                        help='Output directory')
    parser.add_argument('--experiment', '-e',
                        choices=['convergence', 'fault_tolerance', 'noise', 'network_partition', 'all'],
                        default='convergence',
                        help='Experiment type (default: convergence)')
    parser.add_argument('--noise-level', type=int, choices=NOISE_LEVELS,
                        help='Noise level for noise experiments (50, 70, 90)')
    parser.add_argument('--topology', '-t', choices=TOPOLOGIES, default='clique',
                        help='Network topology for PraSLE (default: clique)')
    parser.add_argument('--fast-mode', '-f', action='store_true',
                        help='Enable FAST_MODE in build commands for faster convergence')
    parser.add_argument('--all', action='store_true',
                        help='Generate all combinations')

    args = parser.parse_args()

    algorithms = ALGORITHMS if args.algorithm == 'all' else [args.algorithm]
    node_counts = NODE_COUNTS if args.nodes is None else [args.nodes]
    experiments = ['convergence', 'fault_tolerance', 'noise', 'network_partition'] if args.experiment == 'all' else [args.experiment]
    noise_levels = NOISE_LEVELS if args.noise_level is None else [args.noise_level]

    total_generated = 0

    for exp in experiments:
        mode_str = " (FAST_MODE)" if args.fast_mode else ""
        topo_str = f" [{args.topology} topology]" if args.topology != 'clique' else ""
        print(f"\n=== Generating {exp} CSC files{mode_str}{topo_str} ===")
        count = generate_all_for_experiment(
            args.output, exp, algorithms, node_counts,
            noise_levels if exp == 'noise' else None,
            fast_mode=args.fast_mode,
            topology=args.topology
        )
        total_generated += count

    print(f'\n=== Summary ===')
    print(f'Total generated: {total_generated} CSC files')


if __name__ == '__main__':
    main()
