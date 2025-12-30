#!/bin/bash
# ============================================================
# Leader Election Algorithm Framework - Analysis Script
# ============================================================
#
# Usage:
#   ./analyze.sh                           # Interactive mode
#   ./analyze.sh <algorithm>               # Analyze all experiments for algorithm
#   ./analyze.sh <algorithm> <experiment>  # Analyze specific experiment
#   ./analyze.sh --help                    # Show help
#
# Examples:
#   ./analyze.sh bully                     # Analyze all bully results
#   ./analyze.sh bully convergence         # Analyze bully convergence only
#   ./analyze.sh ring fault_tolerance      # Analyze ring fault tolerance
#   ./analyze.sh all                       # Analyze all algorithms
#
# ============================================================

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "${SCRIPT_DIR}")"
RESULTS_DIR="${PROJECT_DIR}/results"
EXPERIMENTS_DIR="${SCRIPT_DIR}"
SCRIPTS_DIR="${PROJECT_DIR}/scripts"

# Valid options
VALID_ALGORITHMS=("bully" "ring" "prasle" "adaptive-prasle" "all")
VALID_EXPERIMENTS=("convergence" "fault_tolerance" "noise" "network_partition" "all")

# ============================================================
# Helper Functions
# ============================================================

print_header() {
    echo -e "${CYAN}"
    echo "============================================================"
    echo " Leader Election Framework - Analysis Script"
    echo "============================================================"
    echo -e "${NC}"
}

print_usage() {
    echo -e "${YELLOW}Usage:${NC}"
    echo "  ./analyze.sh                           # Interactive mode"
    echo "  ./analyze.sh <algorithm>               # Analyze all experiments"
    echo "  ./analyze.sh <algorithm> <experiment>  # Analyze specific experiment"
    echo "  ./analyze.sh --help                    # Show this help"
    echo ""
    echo -e "${YELLOW}Algorithms:${NC}"
    echo "  bully, ring, prasle, adaptive-prasle, all"
    echo ""
    echo -e "${YELLOW}Experiments:${NC}"
    echo "  convergence, fault_tolerance, noise, network_partition, all"
    echo ""
    echo -e "${YELLOW}Examples:${NC}"
    echo "  ./analyze.sh bully                     # Analyze all bully results"
    echo "  ./analyze.sh bully convergence         # Analyze bully convergence"
    echo "  ./analyze.sh ring fault_tolerance      # Analyze ring fault tolerance"
    echo "  ./analyze.sh all convergence           # Analyze convergence for all algorithms"
    echo ""
    echo -e "${YELLOW}Output:${NC}"
    echo "  Analysis results are saved in the same directory as the input data"
    echo "  Plots are generated as PNG files"
    echo "  Summary statistics are printed to console and saved to CSV"
}

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_step() {
    echo -e "${BLUE}[STEP]${NC} $1"
}

# Check if algorithm is valid
is_valid_algorithm() {
    local algo="$1"
    for valid in "${VALID_ALGORITHMS[@]}"; do
        if [[ "$algo" == "$valid" ]]; then
            return 0
        fi
    done
    return 1
}

# Check if experiment is valid
is_valid_experiment() {
    local exp="$1"
    for valid in "${VALID_EXPERIMENTS[@]}"; do
        if [[ "$exp" == "$valid" ]]; then
            return 0
        fi
    done
    return 1
}

# Find latest result directory for an algorithm/experiment
find_latest_results() {
    local algo="$1"
    local exp="$2"
    local search_dir="${RESULTS_DIR}/${algo}/${exp}"

    if [[ ! -d "$search_dir" ]]; then
        return 1
    fi

    # Find the most recently modified directory
    local latest=$(ls -td "${search_dir}"/*/ 2>/dev/null | head -1)
    if [[ -n "$latest" ]]; then
        echo "$latest"
        return 0
    fi
    return 1
}

# Get CSV file for experiment type
get_csv_file() {
    local exp="$1"
    local result_dir="$2"

    case "$exp" in
        convergence)
            echo "${result_dir}/convergence_times.csv"
            ;;
        fault_tolerance)
            echo "${result_dir}/crash_recovery_times.csv"
            ;;
        noise)
            echo "${result_dir}/noise_convergence_times.csv"
            ;;
        network_partition)
            echo "${result_dir}/partition_times.csv"
            ;;
    esac
}

# ============================================================
# Analysis Functions
# ============================================================

analyze_convergence() {
    local algo="$1"
    local result_dir="$2"

    log_step "Analyzing convergence for ${algo}..."

    local csv_file="${result_dir}/convergence_times.csv"

    if [[ ! -f "$csv_file" ]]; then
        log_warn "No convergence_times.csv found in ${result_dir}"
        return 1
    fi

    # Use plot_convergence_distribution.py for analysis
    local output_plot="${result_dir}/convergence_distribution.png"

    python3 "${SCRIPTS_DIR}/plot_convergence_distribution.py" \
        -i "$csv_file" \
        -o "$output_plot" \
        -a "$algo" 2>/dev/null || {
            log_warn "Plot generation failed, showing basic statistics..."
        }

    # Print basic statistics
    echo ""
    echo -e "${CYAN}=== Convergence Statistics for ${algo} ===${NC}"

    # Use Python for statistics
    python3 - "$csv_file" << 'EOF'
import sys
import csv

csv_file = sys.argv[1]

times = []
with open(csv_file, 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        if 'convergence_time_ms' in row and row['convergence_time_ms']:
            try:
                times.append(float(row['convergence_time_ms']))
            except ValueError:
                pass

if times:
    times.sort()
    n = len(times)
    mean = sum(times) / n
    variance = sum((x - mean) ** 2 for x in times) / n
    std = variance ** 0.5
    median = times[n // 2] if n % 2 else (times[n // 2 - 1] + times[n // 2]) / 2

    print(f"  Trials:     {n}")
    print(f"  Mean:       {mean:.2f} ms")
    print(f"  Std Dev:    {std:.2f} ms")
    print(f"  Median:     {median:.2f} ms")
    print(f"  Min:        {min(times):.2f} ms")
    print(f"  Max:        {max(times):.2f} ms")
    print(f"  Range:      {max(times) - min(times):.2f} ms")
else:
    print("  No valid convergence times found")
EOF

    if [[ -f "$output_plot" ]]; then
        log_info "Plot saved to: ${output_plot}"
    fi

    return 0
}

analyze_fault_tolerance() {
    local algo="$1"
    local result_dir="$2"

    log_step "Analyzing fault tolerance for ${algo}..."

    local csv_file="${result_dir}/crash_recovery_times.csv"
    local analyzer="${EXPERIMENTS_DIR}/fault_tolerance/analyze_recovery.py"

    if [[ ! -f "$csv_file" ]]; then
        log_warn "No crash_recovery_times.csv found in ${result_dir}"
        return 1
    fi

    if [[ -f "$analyzer" ]]; then
        python3 "$analyzer" -i "$csv_file" -a "$algo" -o "$result_dir" 2>/dev/null || {
            log_warn "Analyzer script failed, showing basic statistics..."
        }
    fi

    # Print basic statistics
    echo ""
    echo -e "${CYAN}=== Fault Tolerance Statistics for ${algo} ===${NC}"

    python3 - "$csv_file" << 'EOF'
import sys
import csv

csv_file = sys.argv[1]

recovery_times = []
with open(csv_file, 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        for key in ['recovery_time_ms', 'recovery_time', 'time_ms']:
            if key in row and row[key]:
                try:
                    recovery_times.append(float(row[key]))
                    break
                except ValueError:
                    pass

if recovery_times:
    recovery_times.sort()
    n = len(recovery_times)
    mean = sum(recovery_times) / n
    variance = sum((x - mean) ** 2 for x in recovery_times) / n
    std = variance ** 0.5
    median = recovery_times[n // 2] if n % 2 else (recovery_times[n // 2 - 1] + recovery_times[n // 2]) / 2

    print(f"  Trials:           {n}")
    print(f"  Mean Recovery:    {mean:.2f} ms")
    print(f"  Std Dev:          {std:.2f} ms")
    print(f"  Median:           {median:.2f} ms")
    print(f"  Min:              {min(recovery_times):.2f} ms")
    print(f"  Max:              {max(recovery_times):.2f} ms")
else:
    print("  No valid recovery times found")
EOF

    return 0
}

analyze_noise() {
    local algo="$1"
    local result_dir="$2"

    log_step "Analyzing noise experiment for ${algo}..."

    local csv_file="${result_dir}/noise_convergence_times.csv"
    local analyzer="${EXPERIMENTS_DIR}/noise/analyze_noise.py"

    if [[ ! -f "$csv_file" ]]; then
        log_warn "No noise_convergence_times.csv found in ${result_dir}"
        return 1
    fi

    if [[ -f "$analyzer" ]]; then
        python3 "$analyzer" -i "$csv_file" -a "$algo" -o "$result_dir" 2>/dev/null || {
            log_warn "Analyzer script failed, showing basic statistics..."
        }
    fi

    # Print basic statistics
    echo ""
    echo -e "${CYAN}=== Noise Experiment Statistics for ${algo} ===${NC}"

    python3 - "$csv_file" << 'EOF'
import sys
import csv

csv_file = sys.argv[1]

times = []
with open(csv_file, 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        for key in ['convergence_time_ms', 'time_ms', 'convergence_time']:
            if key in row and row[key]:
                try:
                    times.append(float(row[key]))
                    break
                except ValueError:
                    pass

if times:
    times.sort()
    n = len(times)
    mean = sum(times) / n
    variance = sum((x - mean) ** 2 for x in times) / n
    std = variance ** 0.5
    median = times[n // 2] if n % 2 else (times[n // 2 - 1] + times[n // 2]) / 2

    print(f"  Trials:     {n}")
    print(f"  Mean:       {mean:.2f} ms")
    print(f"  Std Dev:    {std:.2f} ms")
    print(f"  Median:     {median:.2f} ms")
    print(f"  Min:        {min(times):.2f} ms")
    print(f"  Max:        {max(times):.2f} ms")
else:
    print("  No valid convergence times found")
EOF

    return 0
}

analyze_partition() {
    local algo="$1"
    local result_dir="$2"

    log_step "Analyzing network partition for ${algo}..."

    local csv_file="${result_dir}/partition_times.csv"
    local analyzer="${EXPERIMENTS_DIR}/network_partition/analyze_partition.py"

    if [[ ! -f "$csv_file" ]]; then
        log_warn "No partition_times.csv found in ${result_dir}"
        return 1
    fi

    if [[ -f "$analyzer" ]]; then
        python3 "$analyzer" -i "$csv_file" -a "$algo" -o "$result_dir" 2>/dev/null || {
            log_warn "Analyzer script failed, showing basic statistics..."
        }
    fi

    # Print basic statistics
    echo ""
    echo -e "${CYAN}=== Network Partition Statistics for ${algo} ===${NC}"

    python3 - "$csv_file" << 'EOF'
import sys
import csv

csv_file = sys.argv[1]

partition_times = []
healing_times = []
with open(csv_file, 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        for key in ['partition_time_ms', 'partition_time', 'split_time_ms']:
            if key in row and row[key]:
                try:
                    partition_times.append(float(row[key]))
                    break
                except ValueError:
                    pass
        for key in ['healing_time_ms', 'healing_time', 'merge_time_ms']:
            if key in row and row[key]:
                try:
                    healing_times.append(float(row[key]))
                    break
                except ValueError:
                    pass

if partition_times:
    partition_times.sort()
    n = len(partition_times)
    mean = sum(partition_times) / n
    print(f"  Partition Detection:")
    print(f"    Trials:     {n}")
    print(f"    Mean:       {mean:.2f} ms")
    print(f"    Min:        {min(partition_times):.2f} ms")
    print(f"    Max:        {max(partition_times):.2f} ms")

if healing_times:
    healing_times.sort()
    n = len(healing_times)
    mean = sum(healing_times) / n
    print(f"  Partition Healing:")
    print(f"    Trials:     {n}")
    print(f"    Mean:       {mean:.2f} ms")
    print(f"    Min:        {min(healing_times):.2f} ms")
    print(f"    Max:        {max(healing_times):.2f} ms")

if not partition_times and not healing_times:
    print("  No valid partition data found")
EOF

    return 0
}

# Run analysis for a specific experiment
run_analysis() {
    local algo="$1"
    local exp="$2"

    # Find the latest results directory
    local result_dir=$(find_latest_results "$algo" "$exp")

    if [[ -z "$result_dir" ]]; then
        log_warn "No results found for ${algo}/${exp}"
        return 1
    fi

    log_info "Found results: ${result_dir}"

    case "$exp" in
        convergence)
            analyze_convergence "$algo" "$result_dir"
            ;;
        fault_tolerance)
            analyze_fault_tolerance "$algo" "$result_dir"
            ;;
        noise)
            analyze_noise "$algo" "$result_dir"
            ;;
        network_partition)
            analyze_partition "$algo" "$result_dir"
            ;;
    esac
}

# Run all experiments for an algorithm
run_all_experiments() {
    local algo="$1"
    local experiments=("convergence" "fault_tolerance" "noise" "network_partition")
    local found_any=false

    echo ""
    echo -e "${CYAN}============================================================${NC}"
    echo -e "${CYAN} Analyzing all experiments for: ${algo}${NC}"
    echo -e "${CYAN}============================================================${NC}"

    for exp in "${experiments[@]}"; do
        if run_analysis "$algo" "$exp"; then
            found_any=true
        fi
        echo ""
    done

    if [[ "$found_any" == "false" ]]; then
        log_warn "No results found for ${algo}"
        return 1
    fi

    return 0
}

# Interactive mode
interactive_mode() {
    print_header

    echo -e "${YELLOW}Select algorithm to analyze:${NC}"
    echo "  1) bully"
    echo "  2) ring"
    echo "  3) prasle"
    echo "  4) adaptive-prasle"
    echo "  5) all"
    echo ""
    read -p "Enter choice [1-5]: " algo_choice

    case "$algo_choice" in
        1) ALGORITHM="bully" ;;
        2) ALGORITHM="ring" ;;
        3) ALGORITHM="prasle" ;;
        4) ALGORITHM="adaptive-prasle" ;;
        5) ALGORITHM="all" ;;
        *)
            log_error "Invalid choice"
            exit 1
            ;;
    esac

    echo ""
    echo -e "${YELLOW}Select experiment to analyze:${NC}"
    echo "  1) convergence"
    echo "  2) fault_tolerance"
    echo "  3) noise"
    echo "  4) network_partition"
    echo "  5) all"
    echo ""
    read -p "Enter choice [1-5]: " exp_choice

    case "$exp_choice" in
        1) EXPERIMENT="convergence" ;;
        2) EXPERIMENT="fault_tolerance" ;;
        3) EXPERIMENT="noise" ;;
        4) EXPERIMENT="network_partition" ;;
        5) EXPERIMENT="all" ;;
        *)
            log_error "Invalid choice"
            exit 1
            ;;
    esac

    echo ""
    main_analysis "$ALGORITHM" "$EXPERIMENT"
}

# Main analysis dispatcher
main_analysis() {
    local algo="$1"
    local exp="$2"

    local algorithms=()
    local experiments=()

    # Expand "all" for algorithms
    if [[ "$algo" == "all" ]]; then
        algorithms=("bully" "ring" "prasle" "adaptive-prasle")
    else
        algorithms=("$algo")
    fi

    # Expand "all" for experiments
    if [[ "$exp" == "all" ]]; then
        experiments=("convergence" "fault_tolerance" "noise" "network_partition")
    else
        experiments=("$exp")
    fi

    # Run analysis for each combination
    for a in "${algorithms[@]}"; do
        if [[ "${#experiments[@]}" -eq 4 ]]; then
            run_all_experiments "$a"
        else
            for e in "${experiments[@]}"; do
                run_analysis "$a" "$e"
            done
        fi
    done
}

# ============================================================
# Main Entry Point
# ============================================================

# Check for help flag
if [[ "$1" == "--help" || "$1" == "-h" ]]; then
    print_header
    print_usage
    exit 0
fi

# Check if results directory exists
if [[ ! -d "$RESULTS_DIR" ]]; then
    log_error "Results directory not found: ${RESULTS_DIR}"
    echo "Run experiments first using ./create-metrics.sh or experiments/run_all_experiments.sh"
    exit 1
fi

# Parse arguments
if [[ $# -eq 0 ]]; then
    # Interactive mode
    interactive_mode
elif [[ $# -eq 1 ]]; then
    # Analyze all experiments for one algorithm
    ALGORITHM="$1"

    if ! is_valid_algorithm "$ALGORITHM"; then
        log_error "Invalid algorithm: ${ALGORITHM}"
        echo "Valid algorithms: ${VALID_ALGORITHMS[*]}"
        exit 1
    fi

    print_header
    main_analysis "$ALGORITHM" "all"
elif [[ $# -eq 2 ]]; then
    # Analyze specific experiment for algorithm
    ALGORITHM="$1"
    EXPERIMENT="$2"

    if ! is_valid_algorithm "$ALGORITHM"; then
        log_error "Invalid algorithm: ${ALGORITHM}"
        echo "Valid algorithms: ${VALID_ALGORITHMS[*]}"
        exit 1
    fi

    if ! is_valid_experiment "$EXPERIMENT"; then
        log_error "Invalid experiment: ${EXPERIMENT}"
        echo "Valid experiments: ${VALID_EXPERIMENTS[*]}"
        exit 1
    fi

    print_header
    main_analysis "$ALGORITHM" "$EXPERIMENT"
else
    log_error "Too many arguments"
    print_usage
    exit 1
fi

echo ""
log_info "Analysis complete!"
