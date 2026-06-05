#!/bin/bash

################################################################################
# HAL Service Quick Reference
# Copy-paste ready commands for common HAL server operations
################################################################################

# =============================================================================
# FIRST-TIME SETUP
# =============================================================================

# Build the project
make configure && make build

# Install as system service with autostart
sudo ./install-service.sh

# Verify installation
systemctl status hal-server


# =============================================================================
# DAILY OPERATIONS
# =============================================================================

# Check if service is running
systemctl status hal-server

# View real-time logs
journalctl -u hal-server -f

# View last 50 log lines
journalctl -u hal-server -n 50

# Restart service
sudo systemctl restart hal-server

# Stop service
sudo systemctl stop hal-server

# Start service
sudo systemctl start hal-server


# =============================================================================
# TROUBLESHOOTING
# =============================================================================

# Check if port 50051 is listening
ss -tlnp | grep 50051

# Find process using port 50051
lsof -i :50051

# Check service errors
journalctl -u hal-server | grep ERROR

# Full service information
systemctl show hal-server

# Check service file syntax
systemd-analyze verify hal-server.service

# Force stop all HAL processes
pkill -9 hal_server


# =============================================================================
# DEVELOPMENT & TESTING
# =============================================================================

# Run HAL server directly (for development)
./run.sh

# Run with debugging enabled
DEBUG=1 ./run.sh

# Run without rebuilding
./run.sh --no-build

# Run on custom port
./run.sh --port 9090

# Run with custom log file
./run.sh --log /tmp/hal-debug.log

# Run individual test
./build/native/bin/test_cpu


# =============================================================================
# LOGGING & MONITORING
# =============================================================================

# Real-time log monitoring
journalctl -u hal-server -f

# Logs since last boot
journalctl -u hal-server -b

# Logs from last 2 hours
journalctl -u hal-server --since "2 hours ago"

# Follow logs with timestamps
journalctl -u hal-server -f -o short-full

# Search logs for warnings
journalctl -u hal-server | grep -i "WARN\|ERROR"

# Get structured JSON logs
journalctl -u hal-server -o json | jq .

# Check disk usage by service
journalctl -u hal-server --disk-usage


# =============================================================================
# AUTOSTART MANAGEMENT
# =============================================================================

# Enable autostart on boot
sudo systemctl enable hal-server

# Disable autostart (but keep service)
sudo systemctl disable hal-server

# Check if autostart is enabled
systemctl is-enabled hal-server

# List services that run before HAL
systemctl list-dependencies --before hal-server


# =============================================================================
# ADVANCED MANAGEMENT
# =============================================================================

# Edit service configuration (opens in $EDITOR)
sudo systemctl edit hal-server

# Reset service configuration to defaults
sudo systemctl revert hal-server

# Reload systemd changes
sudo systemctl daemon-reload

# Show all service environment variables
systemctl show -p Environment hal-server

# Show service restart count
systemctl show -p NRestarts hal-server

# Check service resource usage
systemctl status hal-server | grep "Memory\|CPU"


# =============================================================================
# UNINSTALL & CLEANUP
# =============================================================================

# Uninstall service (interactive)
sudo ./install-service.sh --uninstall

# Manual service removal
sudo systemctl stop hal-server
sudo systemctl disable hal-server
sudo rm /etc/systemd/system/hal-server.service
sudo systemctl daemon-reload

# Remove installation directory
sudo rm -rf /opt/hal

# Clean build artifacts
make clean


# =============================================================================
# CONFIGURATION EXAMPLES
# =============================================================================

# Run on specific IP address
./run.sh --address 192.168.1.100

# Use custom port
HAL_PORT=9090 ./run.sh

# Enable TLS
export HAL_TLS_CERT=/etc/hal/cert.pem
export HAL_TLS_KEY=/etc/hal/key.pem
./run.sh

# Set custom log location
HAL_LOG_FILE=/var/log/hal-custom.log ./run.sh


# =============================================================================
# DEPLOYMENT WORKFLOW
# =============================================================================

# On development machine (x86_64): Cross-compile for Pi 5
make docker-image
make docker-build

# Deploy to Pi 5 (requires SSH access)
make deploy

# On Pi 5: Install and start service
ssh pi@192.168.1.3 << 'EOF'
  cd /tmp/acu-test
  sudo ./install-service.sh
  sudo systemctl start hal-server
  systemctl status hal-server
EOF

# Verify deployment
ssh pi@192.168.1.3 "journalctl -u hal-server -n 10"


# =============================================================================
# PERFORMANCE TUNING
# =============================================================================

# Set CPU quota (80% of single core)
sudo systemctl edit hal-server
# Add: CPUQuota=80%

# Set memory limit
sudo systemctl edit hal-server
# Add: MemoryMax=500M

# Apply changes
sudo systemctl daemon-reload
sudo systemctl restart hal-server


# =============================================================================
# BACKUP & RESTORE
# =============================================================================

# Backup service configuration
sudo cp /etc/systemd/system/hal-server.service ~/hal-server.service.bak

# Backup HAL installation
sudo tar -czf ~/hal-backup.tar.gz /opt/hal/

# Restore service configuration
sudo cp ~/hal-server.service.bak /etc/systemd/system/hal-server.service
sudo systemctl daemon-reload

# Restore HAL installation
sudo tar -xzf ~/hal-backup.tar.gz -C /
sudo systemctl restart hal-server


# =============================================================================
# HEALTH CHECKS
# =============================================================================

# Check if service is running
systemctl is-active hal-server && echo "Running" || echo "Not running"

# Check if port is listening
ss -tlnp | grep -q 50051 && echo "Port listening" || echo "Port not listening"

# Check recent errors
journalctl -u hal-server --since "1 minute ago" | grep -i error && echo "Has errors" || echo "No errors"

# All-in-one health check
{
  echo "=== HAL Service Health Check ==="
  echo -n "Service status: " && (systemctl is-active hal-server && echo "RUNNING" || echo "STOPPED")
  echo -n "Port 50051: " && (ss -tlnp 2>/dev/null | grep -q 50051 && echo "LISTENING" || echo "NOT LISTENING")
  echo -n "Recent errors: " && (journalctl -u hal-server --since "5 minutes ago" | grep -q ERROR && echo "YES" || echo "NO")
  echo "=== End Health Check ==="
}
