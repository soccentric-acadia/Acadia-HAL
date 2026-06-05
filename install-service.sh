#!/bin/bash

################################################################################
# install-service.sh - HAL Server systemd Service Installation
#
# Purpose: Install HAL server as a systemd service with autostart capability
#
# Features:
#   - Copies HAL application to standard location (/opt/hal)
#   - Sets proper permissions and ownership
#   - Installs systemd service file
#   - Enables autostart on boot
#   - Validates installation
#   - Provides service management commands
#
# Usage:
#   sudo ./install-service.sh [OPTIONS]
#
# OPTIONS:
#   --prefix PREFIX      Installation prefix (default: /opt/hal)
#   --user USER          Service user (default: root)
#   --group GROUP        Service group (default: root)
#   --enable-autostart   Enable service autostart (default: yes)
#   --no-start           Don't start service after installation
#   --dry-run            Show what would be done without making changes
#   --uninstall          Uninstall the service
#
# Environment:
#   HAL_PREFIX           Override installation prefix
#   HAL_SERVICE_USER     Override service user
#   HAL_SERVICE_GROUP    Override service group
#
################################################################################

set -o pipefail

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${SCRIPT_DIR}"
VERSION="1.0"

# Installation defaults
INSTALL_PREFIX="${HAL_PREFIX:-/opt/hal}"
SERVICE_USER="${HAL_SERVICE_USER:-root}"
SERVICE_GROUP="${HAL_SERVICE_GROUP:-root}"
SERVICE_NAME="hal-server"
SYSTEMD_DIR="/etc/systemd/system"
DRY_RUN=false
START_SERVICE=true
UNINSTALL=false

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

################################################################################
# Utility Functions
################################################################################

print_header() {
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${CYAN}  HAL Server systemd Service Installer v${VERSION}${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
}

print_info() {
    echo -e "${GREEN}[INFO]${NC} $*"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $*" >&2
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $*" >&2
}

print_success() {
    echo -e "${GREEN}✓${NC} $*"
}

print_fail() {
    echo -e "${RED}✗${NC} $*"
}

print_step() {
    echo -e "\n${BLUE}→${NC} $*"
}

die() {
    print_error "$*"
    exit 1
}

require_root() {
    if [[ $EUID -ne 0 ]]; then
        die "This script must be run as root (use: sudo $0)"
    fi
}

cmd_exists() {
    command -v "$1" &> /dev/null
}

run_cmd() {
    local cmd="$*"
    if [[ "$DRY_RUN" == "true" ]]; then
        echo "  [DRY-RUN] $cmd"
        return 0
    else
        if eval "$cmd"; then
            return 0
        else
            print_error "Command failed: $cmd"
            return 1
        fi
    fi
}

################################################################################
# Validation Functions
################################################################################

validate_build() {
    print_step "Validating build artifacts..."
    
    local executable="${PROJECT_ROOT}/build/native/bin/hal_server"
    local run_script="${PROJECT_ROOT}/run.sh"
    
    if [[ ! -f "$executable" ]]; then
        print_fail "Executable not found: $executable"
        print_info "Please build the project first: cd ${PROJECT_ROOT} && make build"
        return 1
    else
        print_success "Executable found: $executable"
    fi
    
    if [[ ! -f "$run_script" ]]; then
        print_fail "Run script not found: $run_script"
        return 1
    else
        print_success "Run script found: $run_script"
    fi
    
    return 0
}

validate_systemd() {
    print_step "Validating systemd installation..."
    
    if ! cmd_exists systemctl; then
        die "systemctl not found. This system does not support systemd"
    fi
    print_success "systemd is available"
    
    if [[ ! -d "$SYSTEMD_DIR" ]]; then
        die "systemd directory not found: $SYSTEMD_DIR"
    fi
    print_success "systemd directory: $SYSTEMD_DIR"
    
    return 0
}

validate_permissions() {
    print_step "Validating user/group..."
    
    if ! id "$SERVICE_USER" &>/dev/null; then
        print_fail "User not found: $SERVICE_USER"
        if [[ "$SERVICE_USER" != "root" ]]; then
            print_info "Creating user $SERVICE_USER..."
            run_cmd "useradd -r -s /bin/false -d /opt/hal -m $SERVICE_USER" || return 1
            print_success "User created: $SERVICE_USER"
        else
            return 1
        fi
    else
        print_success "User exists: $SERVICE_USER"
    fi
    
    if ! getent group "$SERVICE_GROUP" &>/dev/null; then
        print_fail "Group not found: $SERVICE_GROUP"
        if [[ "$SERVICE_GROUP" != "root" ]]; then
            print_info "Creating group $SERVICE_GROUP..."
            run_cmd "groupadd -r $SERVICE_GROUP" || return 1
            print_success "Group created: $SERVICE_GROUP"
        else
            return 1
        fi
    else
        print_success "Group exists: $SERVICE_GROUP"
    fi
    
    # Ensure user is in group
    if [[ "$SERVICE_USER" != "root" ]]; then
        run_cmd "usermod -a -G $SERVICE_GROUP $SERVICE_USER" || return 1
    fi
    
    return 0
}

################################################################################
# Installation Functions
################################################################################

create_install_directory() {
    print_step "Creating installation directory: $INSTALL_PREFIX"
    
    if [[ -d "$INSTALL_PREFIX" ]]; then
        print_warn "Directory already exists: $INSTALL_PREFIX"
    else
        run_cmd "mkdir -p '$INSTALL_PREFIX'" || return 1
        print_success "Directory created"
    fi
    
    # Create subdirectories
    for subdir in bin lib include doc; do
        run_cmd "mkdir -p '$INSTALL_PREFIX/$subdir'" || return 1
    done
    
    return 0
}

copy_artifacts() {
    print_step "Copying build artifacts..."
    
    local src_bin="${PROJECT_ROOT}/build/native/bin/hal_server"
    local src_lib="${PROJECT_ROOT}/build/native/lib"
    local src_run="${PROJECT_ROOT}/run.sh"
    
    # Copy executable
    if [[ -f "$src_bin" ]]; then
        run_cmd "cp -v '$src_bin' '$INSTALL_PREFIX/bin/hal_server'" || return 1
        run_cmd "chmod 755 '$INSTALL_PREFIX/bin/hal_server'" || return 1
        print_success "Copied executable: $INSTALL_PREFIX/bin/hal_server"
    fi
    
    # Copy libraries
    if [[ -d "$src_lib" ]]; then
        run_cmd "cp -vr '$src_lib'/* '$INSTALL_PREFIX/lib/' 2>/dev/null || true" || return 1
        run_cmd "chmod 755 '$INSTALL_PREFIX/lib'/*.so* 2>/dev/null || true" || return 1
        print_success "Copied libraries: $INSTALL_PREFIX/lib"
    fi
    
    # Copy run.sh script
    if [[ -f "$src_run" ]]; then
        run_cmd "cp -v '$src_run' '$INSTALL_PREFIX/run.sh'" || return 1
        run_cmd "chmod 755 '$INSTALL_PREFIX/run.sh'" || return 1
        print_success "Copied run script: $INSTALL_PREFIX/run.sh"
    fi
    
    return 0
}

set_permissions() {
    print_step "Setting permissions..."
    
    run_cmd "chown -R $SERVICE_USER:$SERVICE_GROUP '$INSTALL_PREFIX'" || return 1
    run_cmd "chmod 755 '$INSTALL_PREFIX'" || return 1
    run_cmd "chmod 755 '$INSTALL_PREFIX'/bin || true" || return 1
    
    print_success "Permissions set"
    return 0
}

install_systemd_service() {
    print_step "Installing systemd service file..."
    
    local src_service="${PROJECT_ROOT}/hal-server.service"
    local dst_service="${SYSTEMD_DIR}/hal-server.service"
    
    if [[ ! -f "$src_service" ]]; then
        print_error "Service file not found: $src_service"
        print_info "Creating service file..."
        create_service_file "$dst_service" || return 1
    else
        run_cmd "cp -v '$src_service' '$dst_service'" || return 1
    fi
    
    # Update service file with correct paths
    if [[ -f "$dst_service" ]]; then
        # Use sed to update paths in the service file
        run_cmd "sed -i 's|/opt/hal|$INSTALL_PREFIX|g' '$dst_service'" || true
        run_cmd "sed -i 's|User=root|User=$SERVICE_USER|g' '$dst_service'" || true
        run_cmd "sed -i 's|Group=root|Group=$SERVICE_GROUP|g' '$dst_service'" || true
        
        print_success "Service file installed: $dst_service"
    fi
    
    return 0
}

create_service_file() {
    local file="$1"
    
    cat > "$file" << 'EOF'
[Unit]
Description=HAL (Hardware Abstraction Layer) gRPC Server
Documentation=https://github.com/your-org/acadia-hal
After=network-online.target
Wants=network-online.target
Requires=multi-user.target

[Service]
Type=simple
User=root
Group=root
WorkingDirectory=/opt/hal
Environment="PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
Environment="LD_LIBRARY_PATH=/opt/hal/lib:/usr/local/lib:/usr/lib:/lib"

ExecStart=/opt/hal/run.sh --no-build

KillMode=mixed
KillSignal=SIGTERM
TimeoutStopSec=30

Restart=on-failure
RestartSec=5

StartLimitInterval=60
StartLimitBurst=3

StandardOutput=journal
StandardError=journal
SyslogIdentifier=hal_server

[Install]
WantedBy=multi-user.target
Alias=hal.service
EOF
    
    if [[ ! "$DRY_RUN" == "true" ]]; then
        chmod 644 "$file"
    fi
}

reload_systemd() {
    print_step "Reloading systemd configuration..."
    
    run_cmd "systemctl daemon-reload" || return 1
    print_success "systemd reloaded"
    return 0
}

enable_autostart() {
    print_step "Enabling service autostart..."
    
    run_cmd "systemctl enable hal-server.service" || return 1
    print_success "Service enabled for autostart"
    return 0
}

start_service_now() {
    if [[ "$START_SERVICE" != "true" ]] || [[ "$DRY_RUN" == "true" ]]; then
        return 0
    fi
    
    print_step "Starting HAL server service..."
    
    if run_cmd "systemctl start hal-server.service"; then
        print_success "Service started"
        sleep 2
        
        # Check service status
        if systemctl is-active --quiet hal-server.service; then
            print_success "Service is running"
            return 0
        else
            print_fail "Service failed to start"
            print_info "Check logs: journalctl -u hal-server -n 50"
            return 1
        fi
    else
        return 1
    fi
}

################################################################################
# Uninstall Functions
################################################################################

uninstall_service() {
    print_step "Uninstalling HAL server service..."
    
    require_root
    
    print_warn "This will stop and remove the HAL server service"
    read -p "Continue with uninstallation? [y/N] " -n 1 -r
    echo
    
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        print_info "Uninstallation cancelled"
        return 0
    fi
    
    # Stop service
    if systemctl is-active --quiet hal-server.service; then
        print_step "Stopping service..."
        run_cmd "systemctl stop hal-server.service" || true
    fi
    
    # Disable service
    print_step "Disabling service..."
    run_cmd "systemctl disable hal-server.service" || true
    
    # Remove service file
    print_step "Removing service file..."
    run_cmd "rm -f '/etc/systemd/system/hal-server.service'" || true
    
    # Reload systemd
    run_cmd "systemctl daemon-reload" || true
    
    # Remove installation directory (optional)
    read -p "Remove installation directory (${INSTALL_PREFIX})? [y/N] " -n 1 -r
    echo
    
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        print_step "Removing installation directory..."
        run_cmd "rm -rf '${INSTALL_PREFIX}'" || true
        print_success "Installation directory removed"
    fi
    
    print_success "Service uninstalled"
    return 0
}

################################################################################
# Status and Help
################################################################################

print_status() {
    echo ""
    print_step "Installation Summary"
    echo "  Installation Prefix: $INSTALL_PREFIX"
    echo "  Service User:        $SERVICE_USER"
    echo "  Service Group:       $SERVICE_GROUP"
    echo "  Service Name:        $SERVICE_NAME"
    echo "  Systemd Unit:        $SYSTEMD_DIR/hal-server.service"
    echo ""
}

print_usage() {
    cat << EOF
Usage: sudo $0 [OPTIONS]

Install HAL server as a systemd service with autostart support.

OPTIONS:
    --prefix PREFIX        Installation prefix (default: /opt/hal)
    --user USER            Service user (default: root)
    --group GROUP          Service group (default: root)
    --no-autostart         Don't enable autostart
    --no-start             Don't start service after installation
    --dry-run              Show what would be done without changes
    --uninstall            Uninstall the service
    --help                 Show this help message

EXAMPLES:
    # Standard installation with autostart
    sudo $0

    # Install in custom location
    sudo $0 --prefix /usr/local/hal

    # Install without autostart
    sudo $0 --no-autostart

    # Uninstall
    sudo $0 --uninstall

SERVICE MANAGEMENT AFTER INSTALLATION:

    # Start the service
    sudo systemctl start hal-server

    # Stop the service
    sudo systemctl stop hal-server

    # Check status
    systemctl status hal-server

    # View logs
    journalctl -u hal-server -f

    # Restart
    sudo systemctl restart hal-server

    # Disable autostart
    sudo systemctl disable hal-server

    # Enable autostart
    sudo systemctl enable hal-server

EOF
}

print_completion_info() {
    echo ""
    echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${GREEN}  Installation Complete!${NC}"
    echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""
    print_success "HAL server is installed and configured"
    echo ""
    echo "Quick Commands:"
    echo "  Check status:      systemctl status hal-server"
    echo "  View logs:         journalctl -u hal-server -f"
    echo "  Restart:           sudo systemctl restart hal-server"
    echo "  Stop:              sudo systemctl stop hal-server"
    echo ""
}

parse_arguments() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --prefix)
                INSTALL_PREFIX="$2"
                shift 2
                ;;
            --user)
                SERVICE_USER="$2"
                shift 2
                ;;
            --group)
                SERVICE_GROUP="$2"
                shift 2
                ;;
            --no-autostart)
                START_SERVICE=false
                shift
                ;;
            --no-start)
                START_SERVICE=false
                shift
                ;;
            --dry-run)
                DRY_RUN=true
                shift
                ;;
            --uninstall)
                UNINSTALL=true
                shift
                ;;
            --help)
                print_usage
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                print_usage
                exit 1
                ;;
        esac
    done
}

################################################################################
# Main Entry Point
################################################################################

main() {
    print_header
    
    # Parse arguments first
    parse_arguments "$@"
    
    # Handle uninstall
    if [[ "$UNINSTALL" == "true" ]]; then
        require_root
        uninstall_service
        exit $?
    fi
    
    # Standard installation flow
    require_root
    
    if [[ "$DRY_RUN" == "true" ]]; then
        print_warn "Running in DRY-RUN mode - no changes will be made"
    fi
    
    print_status
    
    # Validation
    validate_build || die "Build validation failed"
    validate_systemd || die "systemd validation failed"
    validate_permissions || die "Permission validation failed"
    
    # Installation steps
    create_install_directory || die "Failed to create installation directory"
    copy_artifacts || die "Failed to copy artifacts"
    set_permissions || die "Failed to set permissions"
    install_systemd_service || die "Failed to install service file"
    reload_systemd || die "Failed to reload systemd"
    enable_autostart || die "Failed to enable autostart"
    start_service_now || die "Failed to start service"
    
    print_completion_info
}

# Entry point
main "$@"
