#!/bin/bash

# ============================================================
# Leader Election Algorithm Framework - Interactive Build Script
# ============================================================
# Usage: ./build.sh [algorithm]
#   algorithm: bully, ring, prasle, adaptive-prasle, all, clean
#   If no argument provided, shows interactive menu
# ============================================================

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color
BOLD='\033[1m'

# Valid algorithms
ALGORITHMS=("bully" "ring" "prasle" "adaptive-prasle")

# Print colored output
print_header() {
    echo -e "\n${BLUE}${BOLD}============================================================${NC}"
    echo -e "${BLUE}${BOLD}  Leader Election Algorithm Framework - Build System${NC}"
    echo -e "${BLUE}${BOLD}============================================================${NC}\n"
}

print_success() {
    echo -e "${GREEN}${BOLD}[SUCCESS]${NC} $1"
}

print_error() {
    echo -e "${RED}${BOLD}[ERROR]${NC} $1"
}

print_info() {
    echo -e "${CYAN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# Navigate to script directory
cd "$(dirname "$0")"
SCRIPT_DIR=$(pwd)

# Environment validation
validate_environment() {
    print_info "Validating environment..."

    # Check if Makefile exists
    if [ ! -f "Makefile" ]; then
        print_error "Makefile not found in current directory"
        print_error "Please run this script from the leader-election directory"
        exit 1
    fi

    # Check CONTIKI path
    if [ ! -d "../../Makefile.include" ] && [ ! -f "../../Makefile.include" ]; then
        print_warning "Could not verify Contiki-NG root directory"
        print_warning "Make sure you're in the correct location"
    fi

    # Check for algorithm source files
    if [ ! -d "algorithms" ]; then
        print_error "algorithms/ directory not found"
        exit 1
    fi

    print_success "Environment validated"
}

# Show interactive menu
show_menu() {
    echo -e "${BOLD}Select an algorithm to build:${NC}\n"
    echo -e "  ${CYAN}1)${NC} bully           - Bully Leader Election (IPv6/RPL)"
    echo -e "  ${CYAN}2)${NC} ring            - Ring-based Election (NullNet)"
    echo -e "  ${CYAN}3)${NC} prasle          - PraSLE Algorithm (NullNet)"
    echo -e "  ${CYAN}4)${NC} adaptive-prasle - Adaptive PraSLE (NullNet)"
    echo ""
    echo -e "  ${CYAN}5)${NC} all             - Build all algorithms"
    echo -e "  ${CYAN}6)${NC} clean           - Clean all builds"
    echo -e "  ${CYAN}7)${NC} help            - Show Makefile help"
    echo ""
    echo -e "  ${CYAN}0)${NC} Exit"
    echo ""
    read -p "Enter your choice [1-7, 0 to exit]: " choice

    case $choice in
        1) build_algorithm "bully" ;;
        2) build_algorithm "ring" ;;
        3) build_algorithm "prasle" ;;
        4) build_algorithm "adaptive-prasle" ;;
        5) build_all ;;
        6) clean_all ;;
        7) show_help ;;
        0) echo "Exiting."; exit 0 ;;
        *) print_error "Invalid choice"; show_menu ;;
    esac
}

# Build a specific algorithm
build_algorithm() {
    local algo=$1

    echo ""
    print_info "Building ${BOLD}$algo${NC} algorithm..."
    echo -e "${YELLOW}Running: make ALGORITHM=$algo TARGET=cooja${NC}\n"

    # Clean first
    make ALGORITHM=$algo TARGET=cooja clean 2>/dev/null

    # Build
    if make ALGORITHM=$algo TARGET=cooja; then
        echo ""
        print_success "Build completed for $algo!"
        show_next_steps "$algo"
    else
        echo ""
        print_error "Build failed for $algo"
        echo ""
        echo -e "${YELLOW}Troubleshooting tips:${NC}"
        echo "  - Check that Contiki-NG is properly installed"
        echo "  - Verify the CONTIKI path in Makefile"
        echo "  - Look for missing dependencies in error messages above"
        exit 1
    fi
}

# Build all algorithms
build_all() {
    echo ""
    print_info "Building all algorithms..."

    local failed=()
    local succeeded=()

    for algo in "${ALGORITHMS[@]}"; do
        echo ""
        echo -e "${BLUE}----------------------------------------${NC}"
        print_info "Building $algo..."
        echo -e "${BLUE}----------------------------------------${NC}"

        make ALGORITHM=$algo TARGET=cooja clean 2>/dev/null

        if make ALGORITHM=$algo TARGET=cooja; then
            succeeded+=("$algo")
            print_success "$algo built successfully"
        else
            failed+=("$algo")
            print_error "$algo build failed"
        fi
    done

    # Summary
    echo ""
    echo -e "${BLUE}${BOLD}============================================================${NC}"
    echo -e "${BOLD}Build Summary${NC}"
    echo -e "${BLUE}============================================================${NC}"

    if [ ${#succeeded[@]} -gt 0 ]; then
        print_success "Built: ${succeeded[*]}"
    fi

    if [ ${#failed[@]} -gt 0 ]; then
        print_error "Failed: ${failed[*]}"
        exit 1
    fi

    echo ""
    print_info "All algorithms built successfully!"
    echo ""
    echo -e "${BOLD}Next steps:${NC}"
    echo "  1. Start Cooja: cd ../.. && ./tools/cooja/gradlew run"
    echo "  2. Open a simulation file from experiments/ directory"
}

# Clean all builds
clean_all() {
    echo ""
    print_info "Cleaning all builds..."

    make distclean

    print_success "All builds cleaned"
}

# Show Makefile help
show_help() {
    echo ""
    make help
    echo ""
    read -p "Press Enter to return to menu..."
    show_menu
}

# Show next steps after successful build
show_next_steps() {
    local algo=$1

    echo ""
    echo -e "${BOLD}Next steps:${NC}"
    echo ""
    echo "  1. Start Cooja simulator:"
    echo -e "     ${CYAN}cd ../.. && ./tools/cooja/gradlew run${NC}"
    echo ""
    echo "  2. Open a simulation file:"
    echo -e "     ${CYAN}File -> Open Simulation -> examples/leader-election/experiments/...${NC}"
    echo ""
    echo "  3. Or run experiments directly:"
    echo -e "     ${CYAN}./experiments/run_all_experiments.sh${NC}"
    echo ""
    echo "  Available experiment directories:"
    if [ -d "experiments" ]; then
        for dir in experiments/*/; do
            if [ -d "$dir" ]; then
                echo "     - $dir"
            fi
        done
    fi
}

# Main execution
print_header
validate_environment

# Check for command line argument
if [ $# -gt 0 ]; then
    case $1 in
        bully|ring|prasle|adaptive-prasle)
            build_algorithm "$1"
            ;;
        all)
            build_all
            ;;
        clean)
            clean_all
            ;;
        help|--help|-h)
            show_help
            exit 0
            ;;
        *)
            print_error "Unknown algorithm: $1"
            echo "Valid options: bully, ring, prasle, adaptive-prasle, all, clean, help"
            exit 1
            ;;
    esac
else
    # No argument - show interactive menu
    show_menu
fi
