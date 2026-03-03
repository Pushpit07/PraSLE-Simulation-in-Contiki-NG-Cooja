#!/bin/bash
# ============================================================
# Complete Leader Election Algorithm Evaluation
# ============================================================
#
# This script runs ALL experiments for ALL algorithms and generates
# ALL comparison charts for thesis/publication.
#
# Algorithms (7 total):
#   1. bully
#   2. ring
#   3. prasle (clique topology)
#   4. prasle-line
#   5. prasle-ring
#   6. prasle-mesh
#   7. adaptive-prasle
#
# Node counts: 5, 10, 50, 100
#
# Experiment types:
#   1. convergence - Leader election convergence time
#   2. fault_tolerance - Crash recovery time
#   3. noise - Convergence under packet loss
#   4. network_partition - Split-brain behavior
#
# Usage:
#   ./run_complete_evaluation.sh [options]
#
# Options:
#   --trials, -t        Number of trials per configuration (default: 100)
#   --parallel, -p      Number of parallel jobs (default: auto)
#   --experiments, -e   Comma-separated experiments (default: all)
#   --algorithms, -a    Comma-separated algorithms (default: all)
#   --nodes, -n         Comma-separated node counts (default: 5,10,50,100)
#   --skip-experiments  Skip running experiments, only generate charts
#   --skip-charts       Skip chart generation, only run experiments
#   --dry-run           Preview commands without executing
#   --help, -h          Show this help message
#
# Examples:
#   ./run_complete_evaluation.sh                    # Full evaluation
#   ./run_complete_evaluation.sh -t 50              # 50 trials per config
#   ./run_complete_evaluation.sh --skip-experiments # Only generate charts
#   ./run_complete_evaluation.sh -e convergence -t 10  # Quick convergence test
#
# ============================================================

set -e

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Default configuration
NUM_TRIALS=10
PARALLEL_JOBS=""
EXPERIMENTS="convergence,fault_tolerance,noise,network_partition"
ALGORITHMS="bully,ring,prasle,prasle-line,prasle-ring,prasle-mesh,adaptive-prasle,adaptive-prasle-line,adaptive-prasle-ring,adaptive-prasle-mesh"
NODE_COUNTS="5,10,50,100"
SKIP_EXPERIMENTS=false
SKIP_CHARTS=false
DRY_RUN=false

# Parameter case for experiments
# PARAM_CASE=1: Original paper parameters (default)
# PARAM_CASE=2: Standardized parameters for fair cross-algorithm comparison
PARAM_CASE=${PARAM_CASE:-1}

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# ============================================================
# Helper Functions
# ============================================================

print_header() {
    echo ""
    echo -e "${BLUE}============================================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}============================================================${NC}"
}

print_section() {
    echo ""
    echo -e "${CYAN}------------------------------------------------------------${NC}"
    echo -e "${CYAN}$1${NC}"
    echo -e "${CYAN}------------------------------------------------------------${NC}"
}

print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Clean Ring firmware to force rebuild with different RING_SIZE
# Ring algorithm compiles RING_SIZE into the binary, so we need fresh builds
# for each node count. Must clean both firmware AND object files.
clean_ring_firmware() {
    local build_dir="$PROJECT_DIR/build/cooja"
    if [[ -d "$build_dir" ]]; then
        # Clean firmware
        rm -f "$build_dir"/ring-node.* "$build_dir"/mtype*.cooja 2>/dev/null || true
        # Clean object file to force recompilation with new RING_SIZE
        rm -f "$build_dir"/obj/ring-node.o 2>/dev/null || true
        print_info "Cleaned Ring firmware and object cache"
    fi
}

show_help() {
    echo "Complete Leader Election Algorithm Evaluation"
    echo ""
    echo "This script runs all experiments for all algorithms and generates comparison charts."
    echo ""
    echo "Usage:"
    echo "  $0 [options]"
    echo ""
    echo "Options:"
    echo "  --trials, -t        Number of trials per configuration (default: 100)"
    echo "  --parallel, -p      Number of parallel jobs (default: auto-detect)"
    echo "  --experiments, -e   Comma-separated experiments (default: all)"
    echo "  --algorithms, -a    Comma-separated algorithms (default: all)"
    echo "  --nodes, -n         Comma-separated node counts (default: 5,10,50,100)"
    echo "  --skip-experiments  Skip running experiments, only generate charts"
    echo "  --skip-charts       Skip chart generation, only run experiments"
    echo "  --dry-run           Preview commands without executing"
    echo "  --param-case, -c    Parameter case: 1=original paper, 2=standardized (default: 1)"
    echo "  --help, -h          Show this help message"
    echo ""
    echo "Algorithms:"
    echo "  bully, ring, prasle, prasle-line, prasle-ring, prasle-mesh, adaptive-prasle"
    echo "  Use 'all' for all algorithms (default)"
    echo ""
    echo "Experiments:"
    echo "  convergence, fault_tolerance, noise, network_partition"
    echo "  Use 'all' for all experiments (default)"
    echo ""
    echo "Examples:"
    echo "  $0                                    # Full evaluation (takes many hours)"
    echo "  $0 -t 10 -n 5,10                      # Quick test with 10 trials, 5 and 10 nodes"
    echo "  $0 --skip-experiments                 # Only generate charts from existing data"
    echo "  $0 -e convergence -a bully,ring -t 50 # Convergence only for bully and ring"
}

detect_parallel_jobs() {
    if [[ -z "$PARALLEL_JOBS" ]]; then
        if [[ "$(uname)" == "Darwin" ]]; then
            PARALLEL_JOBS=$(sysctl -n hw.ncpu)
        else
            PARALLEL_JOBS=$(nproc 2>/dev/null || echo 4)
        fi
    fi
    echo "$PARALLEL_JOBS"
}

# Get experiment duration based on algorithm and node count
get_duration() {
    local algo=$1
    local nodes=$2
    local exp=$3

    # Default duration
    local duration=60

    # Base duration adjustment for node count
    # Simulation speed decreases significantly with more nodes
    # (100 nodes runs at ~0.015x-0.038x speed, so 60s wall-clock = ~1-2s sim time)
    case $nodes in
        50)  duration=90 ;;
        100) duration=180 ;;
    esac

    # Ring and line topologies need even more time for message propagation
    if [[ "$algo" == "ring" || "$algo" == "prasle-line" || "$algo" == "adaptive-prasle-line" ]]; then
        case $nodes in
            50)  duration=120 ;;
            100) duration=240 ;;
        esac
    elif [[ "$algo" == "prasle-ring" || "$algo" == "adaptive-prasle-ring" ]]; then
        case $nodes in
            50)  duration=100 ;;
            100) duration=180 ;;
        esac
    fi

    # Fault tolerance needs longer duration (crash + recovery)
    if [[ "$exp" == "fault_tolerance" ]]; then
        duration=$((duration + 60))
    fi

    echo $duration
}

# Parse algorithm name into base algorithm and topology
# e.g., "prasle-line" -> "prasle" "line"
# e.g., "bully" -> "bully" ""
parse_algorithm() {
    local algo=$1
    local base_algo=""
    local topology=""

    case $algo in
        prasle-line)
            base_algo="prasle"
            topology="line"
            ;;
        prasle-ring)
            base_algo="prasle"
            topology="ring"
            ;;
        prasle-mesh)
            base_algo="prasle"
            topology="mesh"
            ;;
        adaptive-prasle-line)
            base_algo="adaptive-prasle"
            topology="line"
            ;;
        adaptive-prasle-ring)
            base_algo="adaptive-prasle"
            topology="ring"
            ;;
        adaptive-prasle-mesh)
            base_algo="adaptive-prasle"
            topology="mesh"
            ;;
        *)
            base_algo="$algo"
            topology=""
            ;;
    esac

    echo "$base_algo" "$topology"
}

# Get CSC filename for an experiment type
get_csc_filename() {
    local exp=$1
    local nodes=$2
    local noise_level=$3

    case $exp in
        convergence)
            echo "${nodes}nodes.csc"
            ;;
        fault_tolerance)
            echo "${nodes}nodes-crash.csc"
            ;;
        noise)
            echo "${nodes}nodes-noise${noise_level}.csc"
            ;;
        network_partition)
            echo "${nodes}nodes-partition.csc"
            ;;
    esac
}

# Check if CSC template exists and generate if missing
check_and_generate_csc() {
    local exp=$1
    local algo=$2
    local nodes=$3
    local csc_filename=$4

    local CSC_DIR="$SCRIPT_DIR/$exp/csc_templates/$algo"
    local CSC_FILE="$CSC_DIR/$csc_filename"

    if [[ ! -f "$CSC_FILE" ]]; then
        print_warn "Missing CSC template: $CSC_FILE"
        print_info "Generating CSC template..."

        if [[ "$DRY_RUN" != "true" ]]; then
            # Parse algorithm to get base algorithm and topology
            read base_algo topology <<< $(parse_algorithm "$algo")

            local topo_flag=""
            if [[ -n "$topology" ]]; then
                topo_flag="--topology $topology"
            fi

            # Generate the CSC template
            python3 "$PROJECT_DIR/scripts/generate_csc.py" \
                --algorithm "$base_algo" \
                --nodes "$nodes" \
                --experiment "$exp" \
                $topo_flag \
                --output "$SCRIPT_DIR/$exp/csc_templates/" 2>/dev/null || {
                print_error "Failed to generate CSC template for $algo $exp $nodes nodes"
                return 1
            }
        else
            echo "  [DRY RUN] python3 generate_csc.py --algorithm $algo --nodes $nodes --experiment $exp"
        fi
    fi
    return 0
}

# Ensure all required CSC templates exist for the experiment run
ensure_csc_templates() {
    print_header "Checking/Generating CSC Templates"

    local missing_count=0
    local generated_count=0

    for algo in "${ALGO_ARRAY[@]}"; do
        for nodes in "${NODE_ARRAY[@]}"; do
            for exp in "${EXP_ARRAY[@]}"; do
                case $exp in
                    convergence)
                        local csc_file=$(get_csc_filename "$exp" "$nodes" "")
                        if check_and_generate_csc "$exp" "$algo" "$nodes" "$csc_file"; then
                            ((generated_count++)) || true
                        else
                            ((missing_count++)) || true
                        fi
                        ;;
                    fault_tolerance)
                        local csc_file=$(get_csc_filename "$exp" "$nodes" "")
                        if check_and_generate_csc "$exp" "$algo" "$nodes" "$csc_file"; then
                            ((generated_count++)) || true
                        else
                            ((missing_count++)) || true
                        fi
                        ;;
                    noise)
                        for noise_level in 50 70 90; do
                            local csc_file=$(get_csc_filename "$exp" "$nodes" "$noise_level")
                            if check_and_generate_csc "$exp" "$algo" "$nodes" "$csc_file"; then
                                ((generated_count++)) || true
                            else
                                ((missing_count++)) || true
                            fi
                        done
                        ;;
                    network_partition)
                        local csc_file=$(get_csc_filename "$exp" "$nodes" "")
                        if check_and_generate_csc "$exp" "$algo" "$nodes" "$csc_file"; then
                            ((generated_count++)) || true
                        else
                            ((missing_count++)) || true
                        fi
                        ;;
                esac
            done
        done
    done

    if [[ $missing_count -gt 0 ]]; then
        print_warn "$missing_count CSC templates could not be generated"
    fi
    print_info "CSC template check complete"
}

# ============================================================
# Parse Arguments
# ============================================================

while [[ $# -gt 0 ]]; do
    case $1 in
        --trials|-t)
            NUM_TRIALS="$2"
            shift 2
            ;;
        --parallel|-p)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        --experiments|-e)
            EXPERIMENTS="$2"
            shift 2
            ;;
        --algorithms|-a)
            ALGORITHMS="$2"
            shift 2
            ;;
        --nodes|-n)
            NODE_COUNTS="$2"
            shift 2
            ;;
        --skip-experiments)
            SKIP_EXPERIMENTS=true
            shift
            ;;
        --skip-charts)
            SKIP_CHARTS=true
            shift
            ;;
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        --param-case|-c)
            PARAM_CASE="$2"
            shift 2
            ;;
        --help|-h)
            show_help
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# ============================================================
# Setup
# ============================================================

# Handle 'all' keyword
if [[ "$ALGORITHMS" == "all" ]]; then
    ALGORITHMS="bully,ring,prasle,prasle-line,prasle-ring,prasle-mesh,adaptive-prasle"
fi

if [[ "$EXPERIMENTS" == "all" ]]; then
    EXPERIMENTS="convergence,fault_tolerance,noise,network_partition"
fi

# Parse into arrays
IFS=',' read -ra ALGO_ARRAY <<< "$ALGORITHMS"
IFS=',' read -ra EXP_ARRAY <<< "$EXPERIMENTS"
IFS=',' read -ra NODE_ARRAY <<< "$NODE_COUNTS"

# Detect parallel jobs
PARALLEL_JOBS=$(detect_parallel_jobs)

# ============================================================
# Print Configuration
# ============================================================

print_header "Complete Leader Election Evaluation"

# Record script start time
SCRIPT_START_TIME=$(date +%s)
SCRIPT_START_DATETIME=$(date "+%Y-%m-%d %H:%M:%S")
echo "Started at: $SCRIPT_START_DATETIME"
echo ""

echo "Configuration:"
echo "  Algorithms:     ${ALGO_ARRAY[*]}"
echo "  Node counts:    ${NODE_ARRAY[*]}"
echo "  Experiments:    ${EXP_ARRAY[*]}"
echo "  Trials:         $NUM_TRIALS"
echo "  Parallel jobs:  $PARALLEL_JOBS"
echo "  Param case:     $PARAM_CASE (1=original paper, 2=standardized)"
echo "  Skip experiments: $SKIP_EXPERIMENTS"
echo "  Skip charts:    $SKIP_CHARTS"
echo "  Dry run:        $DRY_RUN"
echo ""

# Calculate total configurations
TOTAL_CONFIGS=0
for algo in "${ALGO_ARRAY[@]}"; do
    for nodes in "${NODE_ARRAY[@]}"; do
        for exp in "${EXP_ARRAY[@]}"; do
            if [[ "$exp" == "noise" ]]; then
                # Noise has 3 levels (50%, 70%, 90%)
                TOTAL_CONFIGS=$((TOTAL_CONFIGS + 3))
            else
                TOTAL_CONFIGS=$((TOTAL_CONFIGS + 1))
            fi
        done
    done
done

print_info "Total experiment configurations: $TOTAL_CONFIGS"
print_info "Total trial runs: $((TOTAL_CONFIGS * NUM_TRIALS))"

if [[ "$DRY_RUN" == "true" ]]; then
    print_warn "DRY RUN MODE - Commands will be shown but not executed"
fi

# ============================================================
# Ensure CSC Templates Exist
# ============================================================

if [[ "$SKIP_EXPERIMENTS" != "true" ]]; then
    ensure_csc_templates
fi

# ============================================================
# Run Experiments
# ============================================================

if [[ "$SKIP_EXPERIMENTS" != "true" ]]; then
    print_header "Running Experiments"

    # Export PARAM_CASE for subscripts
    export PARAM_CASE

    START_TIME=$(date +%s)

    for exp in "${EXP_ARRAY[@]}"; do
        print_section "Experiment: $exp"

        for algo in "${ALGO_ARRAY[@]}"; do
            for nodes in "${NODE_ARRAY[@]}"; do
                DURATION=$(get_duration "$algo" "$nodes" "$exp")

                print_info "Running $exp for $algo with $nodes nodes (duration: ${DURATION}s)"

                # Ring algorithm needs fresh builds for each RING_SIZE
                if [[ "$algo" == "ring" ]]; then
                    clean_ring_firmware
                fi

                if [[ "$DRY_RUN" == "true" ]]; then
                    case $exp in
                        convergence)
                            echo "  [DRY RUN] ./convergence/run_convergence_trials.sh $algo $nodes $NUM_TRIALS $DURATION $PARALLEL_JOBS"
                            ;;
                        fault_tolerance)
                            CRASH_TIME=$((DURATION / 2))
                            echo "  [DRY RUN] ./fault_tolerance/run_crash_trials.sh $algo $nodes $NUM_TRIALS $CRASH_TIME $DURATION $PARALLEL_JOBS"
                            ;;
                        noise)
                            for noise in 90 70 50; do
                                echo "  [DRY RUN] ./noise/run_noise_trials.sh $algo $nodes $noise $NUM_TRIALS $DURATION $PARALLEL_JOBS"
                            done
                            ;;
                        network_partition)
                            echo "  [DRY RUN] ./network_partition/run_partition_trials.sh $algo $nodes $NUM_TRIALS $DURATION $PARALLEL_JOBS"
                            ;;
                    esac
                else
                    case $exp in
                        convergence)
                            if [[ -x "$SCRIPT_DIR/convergence/run_convergence_trials.sh" ]]; then
                                "$SCRIPT_DIR/convergence/run_convergence_trials.sh" \
                                    "$algo" "$nodes" "$NUM_TRIALS" "$DURATION" "$PARALLEL_JOBS" || {
                                    print_warn "Convergence experiment failed for $algo $nodes nodes"
                                }
                            else
                                print_warn "Convergence script not found"
                            fi
                            ;;
                        fault_tolerance)
                            if [[ -x "$SCRIPT_DIR/fault_tolerance/run_crash_trials.sh" ]]; then
                                CRASH_TIME=$((DURATION / 2))
                                "$SCRIPT_DIR/fault_tolerance/run_crash_trials.sh" \
                                    "$algo" "$nodes" "$NUM_TRIALS" "$CRASH_TIME" "$DURATION" "$PARALLEL_JOBS" || {
                                    print_warn "Fault tolerance experiment failed for $algo $nodes nodes"
                                }
                            else
                                print_warn "Fault tolerance script not found"
                            fi
                            ;;
                        noise)
                            if [[ -x "$SCRIPT_DIR/noise/run_noise_trials.sh" ]]; then
                                for noise in 90 70 50; do
                                    print_info "  Noise level: ${noise}% success rate"
                                    "$SCRIPT_DIR/noise/run_noise_trials.sh" \
                                        "$algo" "$nodes" "$noise" "$NUM_TRIALS" "$DURATION" "$PARALLEL_JOBS" || {
                                        print_warn "Noise experiment failed for $algo $nodes nodes noise=$noise"
                                    }
                                done
                            else
                                print_warn "Noise script not found"
                            fi
                            ;;
                        network_partition)
                            if [[ -x "$SCRIPT_DIR/network_partition/run_partition_trials.sh" ]]; then
                                "$SCRIPT_DIR/network_partition/run_partition_trials.sh" \
                                    "$algo" "$nodes" "$NUM_TRIALS" "$DURATION" "$PARALLEL_JOBS" || {
                                    print_warn "Partition experiment failed for $algo $nodes nodes"
                                }
                            else
                                print_warn "Partition script not found"
                            fi
                            ;;
                    esac
                fi
            done
        done
    done

    END_TIME=$(date +%s)
    EXPERIMENT_TIME=$((END_TIME - START_TIME))
    print_info "Experiments completed in ${EXPERIMENT_TIME}s"
fi

# ============================================================
# Generate Charts
# ============================================================

if [[ "$SKIP_CHARTS" != "true" ]]; then
    print_header "Generating Comparison Charts"

    # Create two chart directories: auto-scale and same-scale
    CHART_DIR_BASE="$PROJECT_DIR/results/case${PARAM_CASE}/comparison_charts"
    CHART_DIR_AUTO="$CHART_DIR_BASE/auto-scale"
    CHART_DIR_SAME="$CHART_DIR_BASE/same-scale"

    if [[ "$DRY_RUN" != "true" ]]; then
        mkdir -p "$CHART_DIR_AUTO"
        mkdir -p "$CHART_DIR_SAME"
    fi

    COMPARISON_DIR="$SCRIPT_DIR/comparison"
    RESULTS_DIR="$PROJECT_DIR/results/case${PARAM_CASE}"

    # Check if Python scripts exist
    if [[ ! -d "$COMPARISON_DIR" ]]; then
        print_error "Comparison scripts directory not found: $COMPARISON_DIR"
        exit 1
    fi

    # Generate convergence charts
    if [[ " ${EXP_ARRAY[*]} " =~ " convergence " ]]; then
        print_section "Convergence Charts"

        if [[ "$DRY_RUN" == "true" ]]; then
            echo "  [DRY RUN] python3 plot_node_comparison_charts.py -r $RESULTS_DIR -m convergence -o $CHART_DIR_AUTO/convergence_by_nodes.png"
            echo "  [DRY RUN] python3 plot_node_comparison_charts.py -r $RESULTS_DIR -m convergence --same-scale -o $CHART_DIR_SAME/convergence_by_nodes.png"
            echo "  [DRY RUN] python3 plot_node_comparison_charts.py -r $RESULTS_DIR -m messages -o $CHART_DIR_AUTO/messages_by_nodes.png"
            echo "  [DRY RUN] python3 plot_node_comparison_charts.py -r $RESULTS_DIR -m messages --same-scale -o $CHART_DIR_SAME/messages_by_nodes.png"
        else
            cd "$COMPARISON_DIR"

            print_info "Generating convergence time comparison (auto-scale)"
            python3 plot_node_comparison_charts.py -r "$RESULTS_DIR" -m convergence -o "$CHART_DIR_AUTO/convergence_by_nodes.png" || {
                print_warn "Failed to generate convergence chart (auto-scale)"
            }
            print_info "Generating convergence time comparison (same-scale)"
            python3 plot_node_comparison_charts.py -r "$RESULTS_DIR" -m convergence --same-scale -o "$CHART_DIR_SAME/convergence_by_nodes.png" || {
                print_warn "Failed to generate convergence chart (same-scale)"
            }

            print_info "Generating message overhead comparison (auto-scale)"
            python3 plot_node_comparison_charts.py -r "$RESULTS_DIR" -m messages -o "$CHART_DIR_AUTO/messages_by_nodes.png" || {
                print_warn "Failed to generate messages chart (auto-scale)"
            }
            print_info "Generating message overhead comparison (same-scale)"
            python3 plot_node_comparison_charts.py -r "$RESULTS_DIR" -m messages --same-scale -o "$CHART_DIR_SAME/messages_by_nodes.png" || {
                print_warn "Failed to generate messages chart (same-scale)"
            }
        fi
    fi

    # Generate fault tolerance charts
    if [[ " ${EXP_ARRAY[*]} " =~ " fault_tolerance " ]]; then
        print_section "Fault Tolerance Charts"

        if [[ "$DRY_RUN" == "true" ]]; then
            echo "  [DRY RUN] python3 plot_recovery_comparison_charts.py -r $RESULTS_DIR -o $CHART_DIR_AUTO/recovery_by_nodes.png"
            echo "  [DRY RUN] python3 plot_recovery_comparison_charts.py -r $RESULTS_DIR --same-scale -o $CHART_DIR_SAME/recovery_by_nodes.png"
            echo "  [DRY RUN] python3 plot_recovery_comparison_charts.py -r $RESULTS_DIR --grid-3x2 -o $CHART_DIR_AUTO/recovery_4x3_grid.png"
            echo "  [DRY RUN] python3 plot_recovery_comparison_charts.py -r $RESULTS_DIR --grid-3x2 --same-scale -o $CHART_DIR_SAME/recovery_4x3_grid.png"
        else
            cd "$COMPARISON_DIR"

            print_info "Generating recovery time comparison 2x2 (auto-scale)"
            python3 plot_recovery_comparison_charts.py -r "$RESULTS_DIR" -o "$CHART_DIR_AUTO/recovery_by_nodes.png" || {
                print_warn "Failed to generate recovery chart (auto-scale)"
            }
            print_info "Generating recovery time comparison 2x2 (same-scale)"
            python3 plot_recovery_comparison_charts.py -r "$RESULTS_DIR" --same-scale -o "$CHART_DIR_SAME/recovery_by_nodes.png" || {
                print_warn "Failed to generate recovery chart (same-scale)"
            }

            print_info "Generating recovery time comparison 4x3 (auto-scale)"
            python3 plot_recovery_comparison_charts.py -r "$RESULTS_DIR" --grid-3x2 -o "$CHART_DIR_AUTO/recovery_4x3_grid.png" || {
                print_warn "Failed to generate recovery 4x3 grid chart (auto-scale)"
            }
            print_info "Generating recovery time comparison 4x3 (same-scale)"
            python3 plot_recovery_comparison_charts.py -r "$RESULTS_DIR" --grid-3x2 --same-scale -o "$CHART_DIR_SAME/recovery_4x3_grid.png" || {
                print_warn "Failed to generate recovery 4x3 grid chart (same-scale)"
            }
        fi
    fi

    # Generate noise charts
    if [[ " ${EXP_ARRAY[*]} " =~ " noise " ]]; then
        print_section "Noise Resilience Charts"

        if [[ "$DRY_RUN" == "true" ]]; then
            for noise in 90 70 50; do
                echo "  [DRY RUN] python3 plot_noise_comparison_charts.py -r $RESULTS_DIR -n $noise -o $CHART_DIR_AUTO/noise${noise}_by_nodes.png"
                echo "  [DRY RUN] python3 plot_noise_comparison_charts.py -r $RESULTS_DIR -n $noise --same-scale -o $CHART_DIR_SAME/noise${noise}_by_nodes.png"
            done
        else
            cd "$COMPARISON_DIR"

            for noise in 90 70 50; do
                packet_loss=$((100 - noise))
                print_info "Generating noise chart ${packet_loss}% loss (auto-scale)"
                python3 plot_noise_comparison_charts.py -r "$RESULTS_DIR" -n "$noise" -o "$CHART_DIR_AUTO/noise${noise}_by_nodes.png" || {
                    print_warn "Failed to generate noise chart for $noise% (auto-scale)"
                }
                print_info "Generating noise chart ${packet_loss}% loss (same-scale)"
                python3 plot_noise_comparison_charts.py -r "$RESULTS_DIR" -n "$noise" --same-scale -o "$CHART_DIR_SAME/noise${noise}_by_nodes.png" || {
                    print_warn "Failed to generate noise chart for $noise% (same-scale)"
                }
            done
        fi
    fi

    # Generate partition charts
    if [[ " ${EXP_ARRAY[*]} " =~ " network_partition " ]]; then
        print_section "Network Partition Charts"

        if [[ "$DRY_RUN" == "true" ]]; then
            echo "  [DRY RUN] python3 plot_partition_comparison_charts.py -r $RESULTS_DIR -o $CHART_DIR_AUTO/partition_by_nodes.png"
            echo "  [DRY RUN] python3 plot_partition_comparison_charts.py -r $RESULTS_DIR --same-scale -o $CHART_DIR_SAME/partition_by_nodes.png"
        else
            cd "$COMPARISON_DIR"

            print_info "Generating partition comparison (auto-scale)"
            python3 plot_partition_comparison_charts.py -r "$RESULTS_DIR" -o "$CHART_DIR_AUTO/partition_by_nodes.png" || {
                print_warn "Failed to generate partition chart (auto-scale)"
            }
            print_info "Generating partition comparison (same-scale)"
            python3 plot_partition_comparison_charts.py -r "$RESULTS_DIR" --same-scale -o "$CHART_DIR_SAME/partition_by_nodes.png" || {
                print_warn "Failed to generate partition chart (same-scale)"
            }
        fi
    fi

    # Generate cross-algorithm comparison (existing script)
    # Note: These are single-plot charts (not subplot grids), so same-scale doesn't apply.
    # We generate in both directories for completeness.
    print_section "Cross-Algorithm Comparison"

    if [[ "$DRY_RUN" == "true" ]]; then
        echo "  [DRY RUN] python3 compare_algorithms.py -r $RESULTS_DIR -o $CHART_DIR_AUTO/cross_algorithm/"
        echo "  [DRY RUN] python3 compare_algorithms.py -r $RESULTS_DIR -o $CHART_DIR_SAME/cross_algorithm/"
    else
        cd "$COMPARISON_DIR"

        if [[ -f "compare_algorithms.py" ]]; then
            print_info "Generating cross-algorithm comparison plots (auto-scale)"
            python3 compare_algorithms.py -r "$RESULTS_DIR" -o "$CHART_DIR_AUTO/cross_algorithm/" || {
                print_warn "Failed to generate cross-algorithm charts (auto-scale)"
            }
            print_info "Generating cross-algorithm comparison plots (same-scale)"
            python3 compare_algorithms.py -r "$RESULTS_DIR" -o "$CHART_DIR_SAME/cross_algorithm/" || {
                print_warn "Failed to generate cross-algorithm charts (same-scale)"
            }
        fi
    fi

    # Generate per-algorithm charts (metrics and scalability for each algorithm)
    # Note: These are single-plot charts (not subplot grids), so same-scale doesn't apply.
    # We generate in both directories for completeness.
    print_section "Per-Algorithm Metrics Charts"

    if [[ "$DRY_RUN" == "true" ]]; then
        echo "  [DRY RUN] python3 plot_per_algorithm_charts.py -r $RESULTS_DIR -o $CHART_DIR_AUTO/per_algorithm/"
        echo "  [DRY RUN] python3 plot_per_algorithm_charts.py -r $RESULTS_DIR -o $CHART_DIR_SAME/per_algorithm/"
    else
        cd "$COMPARISON_DIR"

        if [[ -f "plot_per_algorithm_charts.py" ]]; then
            print_info "Generating per-algorithm metrics and scalability charts (auto-scale)"
            python3 plot_per_algorithm_charts.py -r "$RESULTS_DIR" -o "$CHART_DIR_AUTO/per_algorithm/" || {
                print_warn "Failed to generate per-algorithm charts (auto-scale)"
            }
            print_info "Generating per-algorithm metrics and scalability charts (same-scale)"
            python3 plot_per_algorithm_charts.py -r "$RESULTS_DIR" -o "$CHART_DIR_SAME/per_algorithm/" || {
                print_warn "Failed to generate per-algorithm charts (same-scale)"
            }
        fi
    fi

    # Generate messages vs convergence time trade-off chart
    # Note: These are single-plot charts (not subplot grids), so same-scale doesn't apply.
    # We generate in both directories for completeness.
    print_section "Messages vs Convergence Trade-off Charts"

    if [[ "$DRY_RUN" == "true" ]]; then
        echo "  [DRY RUN] python3 plot_messages_vs_convergence.py -r $RESULTS_DIR -o $CHART_DIR_AUTO/messages_vs_convergence.png"
        echo "  [DRY RUN] python3 plot_messages_vs_convergence.py -r $RESULTS_DIR -o $CHART_DIR_SAME/messages_vs_convergence.png"
    else
        cd "$COMPARISON_DIR"

        if [[ -f "plot_messages_vs_convergence.py" ]]; then
            print_info "Generating messages vs convergence trade-off (auto-scale)"
            python3 plot_messages_vs_convergence.py -r "$RESULTS_DIR" -o "$CHART_DIR_AUTO/messages_vs_convergence.png" || {
                print_warn "Failed to generate messages vs convergence chart (auto-scale)"
            }
            print_info "Generating messages vs convergence trade-off (same-scale)"
            python3 plot_messages_vs_convergence.py -r "$RESULTS_DIR" -o "$CHART_DIR_SAME/messages_vs_convergence.png" || {
                print_warn "Failed to generate messages vs convergence chart (same-scale)"
            }
        fi
    fi

    print_info "Charts saved to:"
    print_info "  Auto-scale: $CHART_DIR_AUTO"
    print_info "  Same-scale: $CHART_DIR_SAME"
fi

# ============================================================
# Summary
# ============================================================

print_header "Evaluation Complete"

echo ""
echo "Summary:"
echo "  Algorithms tested:  ${#ALGO_ARRAY[@]}"
echo "  Node configurations: ${#NODE_ARRAY[@]}"
echo "  Experiment types:   ${#EXP_ARRAY[@]}"
echo "  Trials per config:  $NUM_TRIALS"
echo ""

if [[ "$SKIP_EXPERIMENTS" != "true" && "$DRY_RUN" != "true" ]]; then
    echo "Results saved to:"
    echo "  $PROJECT_DIR/results/"
    echo ""
fi

if [[ "$SKIP_CHARTS" != "true" && "$DRY_RUN" != "true" ]]; then
    CHART_BASE="$PROJECT_DIR/results/case${PARAM_CASE}/comparison_charts"
    echo "Charts saved to:"
    echo "  $CHART_BASE/"
    echo ""
    echo "Auto-scale charts (independent y-axis per subplot):"
    if [[ -d "$CHART_BASE/auto-scale" ]]; then
        ls -1 "$CHART_BASE/auto-scale/"*.png 2>/dev/null | while read f; do
            echo "  - auto-scale/$(basename "$f")"
        done
    fi
    echo ""
    echo "Same-scale charts (uniform y-axis across subplots):"
    if [[ -d "$CHART_BASE/same-scale" ]]; then
        ls -1 "$CHART_BASE/same-scale/"*.png 2>/dev/null | while read f; do
            echo "  - same-scale/$(basename "$f")"
        done
    fi
    echo ""
    echo "Cross-algorithm charts:"
    if [[ -d "$CHART_BASE/auto-scale/cross_algorithm" ]]; then
        ls -1 "$CHART_BASE/auto-scale/cross_algorithm/"*.png 2>/dev/null | while read f; do
            echo "  - auto-scale/cross_algorithm/$(basename "$f")"
        done
    fi
    if [[ -d "$CHART_BASE/same-scale/cross_algorithm" ]]; then
        ls -1 "$CHART_BASE/same-scale/cross_algorithm/"*.png 2>/dev/null | while read f; do
            echo "  - same-scale/cross_algorithm/$(basename "$f")"
        done
    fi
    echo ""
    echo "Per-algorithm charts:"
    if [[ -d "$CHART_BASE/auto-scale/per_algorithm" ]]; then
        ls -1 "$CHART_BASE/auto-scale/per_algorithm/"*.png 2>/dev/null | while read f; do
            echo "  - auto-scale/per_algorithm/$(basename "$f")"
        done
    fi
    if [[ -d "$CHART_BASE/same-scale/per_algorithm" ]]; then
        ls -1 "$CHART_BASE/same-scale/per_algorithm/"*.png 2>/dev/null | while read f; do
            echo "  - same-scale/per_algorithm/$(basename "$f")"
        done
    fi
fi

if [[ "$DRY_RUN" == "true" ]]; then
    print_warn "This was a dry run. No experiments or charts were generated."
fi

# Record script end time
SCRIPT_END_TIME=$(date +%s)
SCRIPT_END_DATETIME=$(date "+%Y-%m-%d %H:%M:%S")
SCRIPT_TOTAL_TIME=$((SCRIPT_END_TIME - SCRIPT_START_TIME))

# Format duration as hours:minutes:seconds
HOURS=$((SCRIPT_TOTAL_TIME / 3600))
MINUTES=$(((SCRIPT_TOTAL_TIME % 3600) / 60))
SECONDS=$((SCRIPT_TOTAL_TIME % 60))

echo ""
echo "Timing:"
echo "  Started:  $SCRIPT_START_DATETIME"
echo "  Finished: $SCRIPT_END_DATETIME"
printf "  Duration: %02d:%02d:%02d (%d seconds)\n" $HOURS $MINUTES $SECONDS $SCRIPT_TOTAL_TIME

echo ""
print_info "Done!"
