#!/bin/bash

################################################################################
# run.sh - HAL Server Lifecycle Manager
# 
# Purpose: Start the HAL (Hardware Abstraction Layer) gRPC server with:
#   - Process monitoring and cleanup
#   - Conflict detection (stop existing instances)
#   - Automatic build verification
#   - Graceful shutdown handling
#   - Logging support
#
# Usage:
#   ./run.sh [--no-build] [--port PORT] [--address ADDRESS] [--log FILE]
#
# Environment Variables:
#   HAL_PORT              - gRPC server port (default: 50051)
#   HAL_ADDRESS           - Server address (default: 0.0.0.0)
#   HAL_TLS_CERT          - TLS certificate path (optional)
#   HAL_TLS_KEY           - TLS key path (optional)
#   HAL_TLS_CA            - TLS CA cert for mutual TLS (optional)
#   HAL_LOG_FILE          - Log file path (optional)
#   HAL_NO_BUILD          - Skip build if set (optional)
#
################################################################################

set -o errexit   # Exit on error
set -o pipefail  # Exit on pipe failure

# Script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${SCRIPT_DIR}"
BUILD_DIR="${PROJECT_ROOT}/build/native"
BIN_DIR="${BUILD_DIR}/bin"
LIB_DIR="${BUILD_DIR}/lib"
EXECUTABLE="${BIN_DIR}/hal_server"
PID_FILE="/var/run/hal_server.pid"
LOCK_FILE="/tmp/hal_server.lock"

# Configuration with defaults
HAL_PORT="${HAL_PORT:-50051}"
HAL_ADDRESS="${HAL_ADDRESS:-0.0.0.0}"
BIND_ADDRESS="${HAL_ADDRESS}:${HAL_PORT}"
LOG_FILE="${HAL_LOG_FILE:-/var/log/hal_server.log}"
NO_BUILD="${HAL_NO_BUILD:-false}"
GRACEFUL_TIMEOUT=10

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

################################################################################
# Logging Functions
################################################################################

log_info() {
    echo -e "${GREEN}[$(date '+%Y-%m-%d %H:%M:%S')]${NC} INFO: $*" | tee -a "${LOG_FILE}" 2>/dev/null || echo "[$(date '+%Y-%m-%d %H:%M:%S')] INFO: $*"
}

log_warn() {
    echo -e "${YELLOW}[$(date '+%Y-%m-%d %H:%M:%S')]${NC} WARN: $*" | tee -a "${LOG_FILE}" 2>/dev/null || echo "[$(date '+%Y-%m-%d %H:%M:%S')] WARN: $*" >&2
}

log_error() {
    echo -e "${RED}[$(date '+%Y-%m-%d %H:%M:%S')]${NC} ERROR: $*" | tee -a "${LOG_FILE}" 2>/dev/null || echo "[$(date '+%Y-%m-%d %H:%M:%S')] ERROR: $*" >&2
}

log_debug() {
    if [[ "${DEBUG:-0}" == "1" ]]; then
        echo -e "${BLUE}[$(date '+%Y-%m-%d %H:%M:%S')]${NC} DEBUG: $*" | tee -a "${LOG_FILE}" 2>/dev/null || echo "[$(date '+%Y-%m-%d %H:%M:%S')] DEBUG: $*"
    fi
}

################################################################################
# Process Management Functions
################################################################################

# Find existing HAL server processes
find_existing_process() {
    local pids=()
    
    # Check by executable path
    if [[ -x "$EXECUTABLE" ]]; then
        pids+=( $(pgrep -f "$(basename "$EXECUTABLE")" 2>/dev/null || true) )
    fi
    
    # Check by port binding
    local port_pids=$(lsof -i ":${HAL_PORT}" -t 2>/dev/null || true)
    if [[ -n "$port_pids" ]]; then
        pids+=( $port_pids )
    fi
    
    # Deduplicate
    if [[ ${#pids[@]} -gt 0 ]]; then
        printf '%s\n' "${pids[@]}" | sort -u
    fi
}

# Check if process is running and healthy
is_process_running() {
    local pid=$1
    if [[ -z "$pid" ]]; then
        return 1
    fi
    
    if kill -0 "$pid" 2>/dev/null; then
        return 0
    else
        return 1
    fi
}

# Stop existing HAL server instances gracefully
stop_existing_processes() {
    local pids=()
    mapfile -t pids < <(find_existing_process)
    
    if [[ ${#pids[@]} -eq 0 ]]; then
        log_debug "No existing HAL server processes found"
        return 0
    fi
    
    log_warn "Found ${#pids[@]} existing HAL server process(es): ${pids[*]}"
    
    for pid in "${pids[@]}"; do
        if [[ -z "$pid" ]]; then
            continue
        fi
        
        log_info "Stopping process $pid (graceful)..."
        
        # Send SIGTERM for graceful shutdown
        if kill -TERM "$pid" 2>/dev/null; then
            local elapsed=0
            while [[ $elapsed -lt $GRACEFUL_TIMEOUT ]]; do
                if ! is_process_running "$pid"; then
                    log_info "Process $pid stopped gracefully"
                    return 0
                fi
                sleep 1
                ((elapsed++))
            done
            
            # Force kill if still running
            log_warn "Process $pid did not stop gracefully, force killing..."
            if kill -KILL "$pid" 2>/dev/null; then
                sleep 1
                if ! is_process_running "$pid"; then
                    log_info "Process $pid killed forcefully"
                else
                    log_error "Failed to kill process $pid"
                    return 1
                fi
            fi
        else
            log_warn "Unable to send signal to process $pid (may already be dead)"
        fi
    done
    
    return 0
}

# Verify executable exists and is executable
verify_executable() {
    if [[ ! -f "$EXECUTABLE" ]]; then
        log_error "Executable not found: $EXECUTABLE"
        log_error "Build directory must be configured and compiled first"
        return 1
    fi
    
    if [[ ! -x "$EXECUTABLE" ]]; then
        log_warn "Making executable: $EXECUTABLE"
        chmod +x "$EXECUTABLE" || {
            log_error "Failed to make executable"
            return 1
        }
    fi
    
    log_info "Verified executable: $EXECUTABLE"
    return 0
}

# Build project if needed
build_project() {
    if [[ "$NO_BUILD" == "true" ]] || [[ -n "$HAL_NO_BUILD" ]]; then
        log_info "Skipping build (--no-build or HAL_NO_BUILD set)"
        return 0
    fi
    
    if verify_executable; then
        log_debug "Executable already built, skipping build"
        return 0
    fi
    
    log_info "Building HAL project..."
    
    if ! command -v cmake &> /dev/null; then
        log_error "cmake not found. Please install CMake"
        return 1
    fi
    
    if ! command -v ninja &> /dev/null && ! command -v make &> /dev/null; then
        log_error "Neither ninja nor make found. Please install one"
        return 1
    fi
    
    # Try using Makefile if available
    if [[ -f "${PROJECT_ROOT}/Makefile" ]]; then
        log_info "Building using Makefile (make build)..."
        if cd "${PROJECT_ROOT}" && make configure build; then
            log_info "Build completed successfully"
            return 0
        else
            log_error "Build failed"
            return 1
        fi
    fi
    
    # Fallback to direct CMake
    log_info "Building using CMake directly..."
    mkdir -p "$BUILD_DIR"
    if cd "$BUILD_DIR" && \
       cmake -G Ninja "${PROJECT_ROOT}" && \
       ninja; then
        log_info "Build completed successfully"
        return 0
    else
        log_error "Build failed"
        return 1
    fi
}

# Validate build configuration
validate_build() {
    local issues=()
    
    if [[ ! -d "$BUILD_DIR" ]]; then
        issues+=("Build directory does not exist: $BUILD_DIR")
    fi
    
    if [[ ! -f "$EXECUTABLE" ]]; then
        issues+=("Executable not found: $EXECUTABLE")
    fi
    
    if [[ ! -d "$LIB_DIR" ]] && [[ "$HAL_ADDRESS" != "127.0.0.1" ]]; then
        issues+=("Library directory not found: $LIB_DIR")
    fi
    
    if [[ ${#issues[@]} -gt 0 ]]; then
        log_error "Build validation failed:"
        for issue in "${issues[@]}"; do
            log_error "  - $issue"
        done
        return 1
    fi
    
    log_info "Build validation passed"
    return 0
}

################################################################################
# Startup and Shutdown
################################################################################

# Setup signal handlers for graceful shutdown
setup_signal_handlers() {
    trap handle_sigterm SIGTERM
    trap handle_sigint SIGINT
    trap cleanup_on_exit EXIT
}

handle_sigterm() {
    log_info "Received SIGTERM, initiating graceful shutdown..."
    kill -TERM "$HAL_PID" 2>/dev/null || true
    wait "$HAL_PID" 2>/dev/null || true
    exit 0
}

handle_sigint() {
    log_info "Received SIGINT (Ctrl+C), initiating graceful shutdown..."
    kill -TERM "$HAL_PID" 2>/dev/null || true
    wait "$HAL_PID" 2>/dev/null || true
    exit 0
}

cleanup_on_exit() {
    log_debug "Cleaning up on exit..."
    
    # Kill HAL process if still running
    if [[ -n "$HAL_PID" ]] && is_process_running "$HAL_PID"; then
        log_info "Stopping HAL server (PID: $HAL_PID)..."
        kill -TERM "$HAL_PID" 2>/dev/null || true
        sleep 1
        kill -KILL "$HAL_PID" 2>/dev/null || true
    fi
    
    # Remove lock file
    if [[ -f "$LOCK_FILE" ]]; then
        rm -f "$LOCK_FILE"
    fi
}

# Set up logging directory
setup_logging() {
    local log_dir
    log_dir=$(dirname "$LOG_FILE")
    
    if [[ ! -d "$log_dir" ]]; then
        if ! mkdir -p "$log_dir" 2>/dev/null; then
            log_warn "Cannot create log directory: $log_dir (will log to stderr only)"
            LOG_FILE="/dev/null"
            return 1
        fi
    fi
    
    if [[ ! -w "$log_dir" ]]; then
        log_warn "Cannot write to log directory: $log_dir (will log to stderr only)"
        LOG_FILE="/dev/null"
        return 1
    fi
    
    # Initialize log file
    {
        echo "================================================================================"
        echo "HAL Server Started: $(date '+%Y-%m-%d %H:%M:%S')"
        echo "================================================================================"
        echo "Address:    ${BIND_ADDRESS}"
        echo "Executable: ${EXECUTABLE}"
        echo "Process:    $$"
        echo "================================================================================"
    } >> "$LOG_FILE" 2>/dev/null || true
    
    log_info "Logging to: $LOG_FILE"
}

# Start the HAL server
start_server() {
    log_info "Starting HAL server..."
    log_info "  Address: ${BIND_ADDRESS}"
    log_info "  Executable: ${EXECUTABLE}"
    
    # Export library path for runtime linking
    export LD_LIBRARY_PATH="${LIB_DIR}:${LD_LIBRARY_PATH:-}"
    
    # Export TLS configuration if provided
    [[ -n "${HAL_TLS_CERT:-}" ]] && export HAL_TLS_CERT
    [[ -n "${HAL_TLS_KEY:-}" ]] && export HAL_TLS_KEY
    [[ -n "${HAL_TLS_CA:-}" ]] && export HAL_TLS_CA
    
    # Log environment
    log_debug "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
    [[ -n "${HAL_TLS_CERT:-}" ]] && log_debug "TLS enabled (cert: ${HAL_TLS_CERT})"
    
    # Start server with address binding
    if [[ "$BIND_ADDRESS" != "0.0.0.0:50051" ]]; then
        log_info "Starting with custom binding: $BIND_ADDRESS"
        "$EXECUTABLE" "$BIND_ADDRESS" >> "$LOG_FILE" 2>&1 &
    else
        log_info "Starting with default binding (0.0.0.0:50051)"
        "$EXECUTABLE" >> "$LOG_FILE" 2>&1 &
    fi
    
    HAL_PID=$!
    log_info "HAL server started with PID: $HAL_PID"
    
    # Verify process is running
    sleep 1
    if ! is_process_running "$HAL_PID"; then
        log_error "HAL server failed to start. Check logs:"
        tail -20 "$LOG_FILE" 2>/dev/null || true
        return 1
    fi
    
    echo "$HAL_PID" > "$PID_FILE" 2>/dev/null || true
    
    # Display status
    show_status
}

# Display server status
show_status() {
    if is_process_running "$HAL_PID"; then
        echo -e "${GREEN}✓ HAL Server is running${NC}"
        echo "  PID:     $HAL_PID"
        echo "  Address: ${BIND_ADDRESS}"
        echo "  Log:     ${LOG_FILE}"
        
        # Try to verify port is listening
        if command -v ss &> /dev/null; then
            if ss -tlnp 2>/dev/null | grep -q "${HAL_PORT}"; then
                echo "  Status:  Listening on port ${HAL_PORT}"
            fi
        elif command -v netstat &> /dev/null; then
            if netstat -tlnp 2>/dev/null | grep -q "${HAL_PORT}"; then
                echo "  Status:  Listening on port ${HAL_PORT}"
            fi
        fi
    else
        echo -e "${RED}✗ HAL Server is not running${NC}"
        return 1
    fi
}

################################################################################
# Help and Parse Arguments
################################################################################

print_usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Start the HAL (Hardware Abstraction Layer) gRPC server with intelligent
process lifecycle management, conflict detection, and automatic builds.

OPTIONS:
    --no-build              Skip build step if not already compiled
    --port PORT             gRPC server port (default: 50051)
    --address ADDRESS       Server bind address (default: 0.0.0.0)
    --log FILE              Log file path (default: /var/log/hal_server.log)
    --help                  Show this help message
    --version               Show version information
    --debug                 Enable debug logging

ENVIRONMENT VARIABLES:
    HAL_PORT                Server port (overrides --port)
    HAL_ADDRESS             Server address (overrides --address)
    HAL_LOG_FILE            Log file path (overrides --log)
    HAL_NO_BUILD            Skip build if set to true
    HAL_TLS_CERT            TLS certificate file path
    HAL_TLS_KEY             TLS private key file path
    HAL_TLS_CA              TLS CA certificate for mutual TLS

EXAMPLES:
    # Start with defaults
    ./run.sh

    # Start on custom port without rebuilding
    ./run.sh --no-build --port 9090

    # Start with TLS
    HAL_TLS_CERT=/etc/hal/cert.pem \\
    HAL_TLS_KEY=/etc/hal/key.pem \\
    ./run.sh

    # Enable debug logging
    DEBUG=1 ./run.sh --debug

EOF
}

print_version() {
    echo "HAL Server Runner v1.0"
    echo "Target: Raspberry Pi 5 (ARM64)"
    echo "Project: Acadia-HAL"
}

parse_arguments() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --no-build)
                NO_BUILD="true"
                shift
                ;;
            --port)
                HAL_PORT="$2"
                shift 2
                ;;
            --address)
                HAL_ADDRESS="$2"
                shift 2
                ;;
            --log)
                LOG_FILE="$2"
                shift 2
                ;;
            --debug)
                DEBUG=1
                shift
                ;;
            --help)
                print_usage
                exit 0
                ;;
            --version)
                print_version
                exit 0
                ;;
            *)
                log_error "Unknown option: $1"
                print_usage
                exit 1
                ;;
        esac
    done
    
    # Recalculate bind address
    BIND_ADDRESS="${HAL_ADDRESS}:${HAL_PORT}"
}

################################################################################
# Main Entry Point
################################################################################

main() {
    log_info "HAL Server Lifecycle Manager"
    log_info "=============================="
    
    # Setup signal handlers and logging
    setup_signal_handlers
    setup_logging
    
    # Parse command-line arguments
    parse_arguments "$@"
    
    log_debug "Configuration:"
    log_debug "  Project Root: $PROJECT_ROOT"
    log_debug "  Build Dir:    $BUILD_DIR"
    log_debug "  Executable:   $EXECUTABLE"
    log_debug "  Address:      ${HAL_ADDRESS}"
    log_debug "  Port:         ${HAL_PORT}"
    log_debug "  No Build:     ${NO_BUILD}"
    
    # Step 1: Stop existing processes
    log_info "Step 1: Checking for existing HAL server instances..."
    if ! stop_existing_processes; then
        log_error "Failed to stop existing processes"
        exit 1
    fi
    
    # Step 2: Build if needed
    log_info "Step 2: Build verification..."
    if ! build_project; then
        log_error "Build failed"
        exit 1
    fi
    
    # Step 3: Validate build
    log_info "Step 3: Build validation..."
    if ! validate_build; then
        log_error "Build validation failed"
        exit 1
    fi
    
    # Step 4: Verify executable
    log_info "Step 4: Verifying executable..."
    if ! verify_executable; then
        log_error "Executable verification failed"
        exit 1
    fi
    
    # Step 5: Start server
    log_info "Step 5: Starting HAL server..."
    if ! start_server; then
        log_error "Failed to start HAL server"
        exit 1
    fi
    
    log_info "HAL server startup complete - monitoring process..."
    
    # Keep script running to monitor HAL process
    while is_process_running "$HAL_PID"; do
        sleep 5
    done
    
    log_error "HAL server process exited unexpectedly"
    tail -30 "$LOG_FILE" 2>/dev/null || true
    exit 1
}

# Run main function
main "$@"
