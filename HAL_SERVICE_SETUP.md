# HAL Server Deployment & Service Management

Comprehensive guide for running the HAL (Hardware Abstraction Layer) gRPC server with systemd service management, autostart capability, and intelligent process lifecycle management.

---

## 📋 Overview

This setup provides three components:

1. **run.sh** - Intelligent process lifecycle manager
   - Stops existing HAL server instances
   - Auto-builds if needed
   - Starts with proper error handling
   - Monitors process health

2. **hal-server.service** - systemd service unit
   - Auto-starts on system boot
   - Automatic restart on failure
   - Proper process management
   - Journal logging integration

3. **install-service.sh** - Service installation script
   - Deploys HAL to standard location
   - Configures systemd service
   - Sets permissions and ownership
   - Validates installation

---

## 🚀 Quick Start

### Option 1: Direct Execution (Development/Testing)

```bash
# Make script executable
chmod +x run.sh

# Run HAL server directly
./run.sh

# Or with custom options
./run.sh --port 9090 --no-build
```

### Option 2: systemd Service (Production)

```bash
# 1. Install service (requires root)
sudo ./install-service.sh

# 2. Start service
sudo systemctl start hal-server

# 3. Check status
systemctl status hal-server

# 4. View logs
journalctl -u hal-server -f
```

---

## 📚 Detailed Usage

### run.sh Script

**Purpose**: Start HAL server with intelligent lifecycle management

#### Basic Usage

```bash
./run.sh                          # Start with defaults
./run.sh --help                   # Show help
./run.sh --version                # Show version
```

#### Command-Line Options

```bash
--no-build              # Skip build step (use existing binary)
--port PORT             # Set gRPC server port (default: 50051)
--address ADDRESS       # Set server bind address (default: 0.0.0.0)
--log FILE              # Set log file path (default: /var/log/hal_server.log)
--debug                 # Enable debug logging
--help                  # Show help message
--version               # Show version info
```

#### Examples

```bash
# Start on port 9090 without rebuilding
./run.sh --no-build --port 9090

# Start with debug logging
DEBUG=1 ./run.sh --debug

# Start with custom address
./run.sh --address 127.0.0.1 --port 8080

# Start with custom log file
./run.sh --log /tmp/hal.log
```

#### Environment Variables

```bash
HAL_PORT              # Server port (default: 50051)
HAL_ADDRESS           # Server address (default: 0.0.0.0)
HAL_LOG_FILE          # Log file path
HAL_NO_BUILD          # Set to true to skip build
HAL_TLS_CERT          # Path to TLS certificate
HAL_TLS_KEY           # Path to TLS private key
HAL_TLS_CA            # Path to TLS CA certificate
```

#### Example with TLS

```bash
export HAL_TLS_CERT=/etc/hal/cert.pem
export HAL_TLS_KEY=/etc/hal/key.pem
export HAL_TLS_CA=/etc/hal/ca.pem
./run.sh --port 50051
```

#### Features

- **Process Detection**: Finds and stops existing HAL instances
  - Scans by process name
  - Checks port binding
  - Graceful termination with timeout
  
- **Automatic Building**: Builds project if executable missing
  - Uses Makefile if available
  - Falls back to CMake + Ninja
  - Validates build output

- **Graceful Shutdown**: Handles signals properly
  - SIGTERM: Graceful shutdown (10-second timeout)
  - SIGINT (Ctrl+C): Graceful shutdown
  - EXIT: Cleanup and process termination

- **Logging**: Comprehensive logging support
  - File logging with timestamps
  - Debug mode for troubleshooting
  - Color-coded console output

---

### install-service.sh Script

**Purpose**: Install HAL server as systemd service with autostart

#### Prerequisites

```bash
# Must be run as root
sudo ./install-service.sh

# System must have systemd
systemctl --version
```

#### Command-Line Options

```bash
--prefix PREFIX        # Installation directory (default: /opt/hal)
--user USER            # Service user (default: root)
--group GROUP          # Service group (default: root)
--no-autostart         # Don't enable autostart
--no-start             # Don't start after installation
--dry-run              # Show changes without applying
--uninstall            # Uninstall the service
--help                 # Show help message
```

#### Installation Examples

```bash
# Standard installation
sudo ./install-service.sh

# Install in custom location
sudo ./install-service.sh --prefix /usr/local/hal

# Install without autostart
sudo ./install-service.sh --no-autostart

# Dry-run to see what would happen
sudo ./install-service.sh --dry-run

# Install as different user
sudo ./install-service.sh --user hal-server --group hal-server
```

#### Uninstallation

```bash
# Remove service and all artifacts
sudo ./install-service.sh --uninstall
```

#### Installation Steps

1. **Validation**
   - Checks executable exists
   - Verifies systemd availability
   - Validates user/group

2. **Setup**
   - Creates installation directory
   - Copies executables and libraries
   - Sets proper ownership and permissions

3. **Service Registration**
   - Installs systemd service file
   - Updates paths in service configuration
   - Reloads systemd daemon

4. **Activation**
   - Enables autostart
   - Starts service immediately
   - Verifies service is running

---

### hal-server.service

**Purpose**: systemd service unit for HAL server

#### Service Configuration

```ini
[Unit]
Description=HAL (Hardware Abstraction Layer) gRPC Server
After=network-online.target
Wants=network-online.target
Requires=multi-user.target

[Service]
Type=simple
User=root
Group=root
WorkingDirectory=/opt/hal
ExecStart=/opt/hal/run.sh --no-build

# Restart policy
Restart=on-failure
RestartSec=5
StartLimitInterval=60
StartLimitBurst=3

# Logging
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

#### Service Management Commands

```bash
# Start service
sudo systemctl start hal-server

# Stop service
sudo systemctl stop hal-server

# Restart service
sudo systemctl restart hal-server

# Check status
systemctl status hal-server

# Enable autostart
sudo systemctl enable hal-server

# Disable autostart
sudo systemctl disable hal-server

# View logs (last 50 lines)
journalctl -u hal-server -n 50

# View logs in real-time
journalctl -u hal-server -f

# View logs since last boot
journalctl -u hal-server -b

# View logs for specific time
journalctl -u hal-server --since "2 hours ago"

# Check if service is enabled
systemctl is-enabled hal-server

# Check if service is active
systemctl is-active hal-server
```

---

## 🔧 Setup & Configuration

### Build the Project

```bash
cd /path/to/Acadia-HAL

# Build using Makefile
make configure
make build

# Or use Docker for cross-compilation (recommended)
make docker-image
make docker-build
```

### Verify Build Output

```bash
# Check executable
ls -la build/native/bin/hal_server

# Check libraries
ls -la build/native/lib/

# List all built binaries
make list
```

### Install as Service

```bash
# 1. Build the project first
make build

# 2. Install service
sudo ./install-service.sh

# 3. Verify installation
systemctl status hal-server
```

---

## 📊 Monitoring & Troubleshooting

### Check Service Status

```bash
# Quick status check
systemctl status hal-server

# Detailed status
systemctl show hal-server

# Check if listening on port
ss -tlnp | grep 50051
# or
lsof -i :50051
```

### View Logs

```bash
# Real-time logs
journalctl -u hal-server -f

# Last 100 lines
journalctl -u hal-server -n 100

# Search for errors
journalctl -u hal-server | grep ERROR

# Full log with timestamps
journalctl -u hal-server -o short-full
```

### Common Issues

#### Service fails to start

```bash
# Check error logs
journalctl -u hal-server -n 50 | tail -30

# Check service file syntax
systemd-analyze verify hal-server.service

# Run service manually for debugging
cd /opt/hal && ./run.sh --debug
```

#### Port already in use

```bash
# Find process using port
lsof -i :50051

# Kill existing process
kill -9 <PID>

# Or use different port
sudo systemctl stop hal-server
sudo ./install-service.sh --no-start
HAL_PORT=9090 sudo systemctl start hal-server
```

#### Permission issues

```bash
# Check file permissions
ls -la /opt/hal/

# Fix ownership
sudo chown -R root:root /opt/hal/

# Check service user
systemctl show -p User hal-server
```

#### High memory usage

```bash
# Check memory usage
ps aux | grep hal_server

# Check resource limits
systemctl show -p MemoryMax hal-server

# Set memory limit
sudo systemctl edit hal-server
# Add: MemoryMax=500M
```

---

## 🔐 Security Considerations

### TLS/SSL Support

```bash
# Generate self-signed certificates
openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 365

# Use with HAL server
export HAL_TLS_CERT=/path/to/cert.pem
export HAL_TLS_KEY=/path/to/key.pem
./run.sh
```

### Service Isolation

The systemd service includes security hardening:

```ini
[Service]
# Temporary files isolation
PrivateTmp=yes

# No new privileges
NoNewPrivileges=true

# Home directory protection
ProtectHome=yes

# System protection
ProtectSystem=strict
ReadWritePaths=/opt/hal /var/log /var/run /dev /proc /sys
```

### Capability Management

```ini
# Required capabilities for hardware access
AmbientCapabilities=CAP_SYS_ADMIN CAP_SYS_NICE CAP_NET_ADMIN
CapabilityBoundingSet=CAP_SYS_ADMIN CAP_SYS_NICE CAP_NET_ADMIN
```

---

## 📝 Performance Tuning

### CPU & Memory Limits

Edit service configuration:

```bash
sudo systemctl edit hal-server
```

Add:

```ini
[Service]
# CPU quota (80% of single core)
CPUQuota=80%

# Memory limit
MemoryMax=500M

# Max tasks
TasksMax=1000
```

### System Integration

```bash
# Check boot order
systemctl list-dependencies --before hal-server

# Check dependencies
systemctl list-dependencies hal-server

# Analyze boot time
systemd-analyze critical-chain hal-server
```

---

## 🔄 Backup & Restore

### Backup Configuration

```bash
# Backup service file
sudo cp /etc/systemd/system/hal-server.service ~/hal-server.service.bak

# Backup HAL installation
sudo tar -czf ~/hal-backup.tar.gz /opt/hal/
```

### Restore Configuration

```bash
# Restore service file
sudo cp ~/hal-server.service.bak /etc/systemd/system/hal-server.service
sudo systemctl daemon-reload

# Restore HAL installation
sudo tar -xzf ~/hal-backup.tar.gz -C /
sudo systemctl restart hal-server
```

---

## 📦 Deployment Workflow

### Development Machine (x86_64)

```bash
# 1. Build for Raspberry Pi 5 (ARM64)
make docker-build

# 2. Deploy to Pi 5
make deploy

# 3. Install service on Pi 5
ssh pi@192.168.1.3 "cd /tmp/acu-test && sudo ./install-service.sh"

# 4. Verify
ssh pi@192.168.1.3 "systemctl status hal-server"
```

### On Raspberry Pi 5 (ARM64)

```bash
# 1. Clone repository
git clone <repo> && cd Acadia-HAL

# 2. Build
make build

# 3. Install service
sudo ./install-service.sh

# 4. Enable autostart
sudo systemctl enable hal-server

# 5. Start service
sudo systemctl start hal-server

# 6. Verify
systemctl status hal-server
journalctl -u hal-server -f
```

---

## 🧪 Testing

### Unit Testing

```bash
# Run all module tests
make test

# Run specific test
./build/native/bin/test_cpu

# Run with verbose output
DEBUG=1 ./build/native/bin/test_cpu
```

### Service Testing

```bash
# Test service start
sudo systemctl start hal-server

# Verify startup
sleep 2 && systemctl is-active hal-server

# Test graceful restart
sudo systemctl restart hal-server

# Verify logs
journalctl -u hal-server --since "1 minute ago"
```

### Load Testing

```bash
# Connect to gRPC server
grpcurl -plaintext list localhost:50051

# Test with custom client
cd tests && make run
```

---

## 📊 Health Checks

### Manual Health Check

```bash
# Check if service is running
systemctl is-active hal-server || echo "Service not running"

# Check if port is listening
ss -tlnp | grep 50051 && echo "Port 50051 is listening"

# Check recent logs for errors
journalctl -u hal-server --since "1 minute ago" | grep -i error || echo "No recent errors"
```

### Automated Health Check Script

Create `/usr/local/bin/check-hal-health.sh`:

```bash
#!/bin/bash
if ! systemctl is-active --quiet hal-server; then
    echo "HAL service is not running"
    systemctl restart hal-server
fi

if ! ss -tlnp | grep -q 50051; then
    echo "HAL port 50051 is not listening"
    systemctl restart hal-server
fi

echo "HAL service is healthy"
```

Add to crontab:

```bash
sudo crontab -e
# Add: */5 * * * * /usr/local/bin/check-hal-health.sh
```

---

## 🎯 Summary

### Quick Reference

```bash
# First-time setup
sudo ./install-service.sh                    # Install with autostart

# Daily operations
systemctl status hal-server                  # Check status
journalctl -u hal-server -f                  # View logs
sudo systemctl restart hal-server            # Restart if needed

# Development
./run.sh --no-build --debug                  # Run with debugging
DEBUG=1 ./run.sh                              # Enable debug logs

# Maintenance
sudo systemctl stop hal-server                # Stop service
sudo ./install-service.sh --uninstall        # Remove service

# Troubleshooting
systemctl show hal-server                    # Full service info
systemd-analyze verify hal-server.service   # Check config syntax
lsof -i :50051                                # Check port binding
```

---

## 📞 Support & Debugging

For detailed logs and debugging:

```bash
# Get full service information
systemctl show hal-server --all

# View service file location
systemctl show hal-server -p FragmentPath

# Check systemd journal for all details
journalctl -u hal-server -o json --all | jq .
```

For more information:
- [systemd.service man page](https://man7.org/linux/man-pages/man5/systemd.service.5.html)
- [journalctl documentation](https://man7.org/linux/man-pages/man1/journalctl.1.html)
- [Project README](README.md)

