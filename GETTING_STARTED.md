# 🚀 HAL Server - Getting Started (5-Minute Quick Start)

## For Impatient Developers

Want to get HAL running **right now**? Follow these 5 minutes of commands.

### Development (Test it locally first)

```bash
cd /home/sandeshghimire/workspace/Acadia/Acadia-HAL

# 1. Build the project (takes 2-3 minutes)
make build

# 2. Run the server directly
./run.sh

# Output should show:
# ✓ HAL Server is running
# PID: XXXX
# Address: 0.0.0.0:50051
# Status: Listening on port 50051

# 3. In another terminal, verify it's running
ss -tlnp | grep 50051

# 4. Stop the server
# Press Ctrl+C in the first terminal
```

**That's it!** HAL is running. 🎉

### Production (Install as system service)

```bash
# Make sure build is done from above
make build

# 1. Install as systemd service (requires sudo)
sudo ./install-service.sh

# Prompts will guide you through installation

# 2. Verify it's running
systemctl status hal-server

# 3. View logs
journalctl -u hal-server -f

# 4. To stop it later
sudo systemctl stop hal-server

# 5. To disable autostart
sudo systemctl disable hal-server
```

**Done!** Service will auto-start on system boot. 🎉

---

## Troubleshooting in 30 Seconds

**Server won't start?**
```bash
# See what's wrong
./run.sh --debug

# Or check logs
journalctl -u hal-server | grep ERROR
```

**Port already in use?**
```bash
# Find what's using it
lsof -i :50051

# Kill it
kill -9 <PID>

# Retry
./run.sh
```

**Need to check if it's running?**
```bash
# Quick status check
systemctl status hal-server
ss -tlnp | grep 50051
```

---

## What Was Created

| File | Purpose | Size |
|------|---------|------|
| **run.sh** | Start HAL with auto-conflict detection | 17 KB |
| **install-service.sh** | Install as systemd service | 17 KB |
| **hal-server.service** | systemd service configuration | 2.2 KB |
| **HAL_SERVICE_SETUP.md** | Full documentation (726 lines) | 14 KB |
| **HAL_QUICK_REFERENCE.sh** | Command reference (275 lines) | 7.3 KB |

---

## Common Commands You'll Need

```bash
# Start/stop/restart
./run.sh                          # Development: direct start
sudo systemctl start hal-server   # Production: via systemd
sudo systemctl stop hal-server    # Stop service
sudo systemctl restart hal-server # Restart service

# Check status
systemctl status hal-server       # Full status
ss -tlnp | grep 50051            # Check if listening

# View logs
journalctl -u hal-server -f       # Real-time logs
journalctl -u hal-server -n 50    # Last 50 lines

# Build
make build                        # Rebuild
make clean && make build          # Clean rebuild

# Install/uninstall service
sudo ./install-service.sh         # Install
sudo ./install-service.sh --uninstall  # Uninstall
```

---

## Configuration Options

### Custom Port
```bash
./run.sh --port 9090
# or
HAL_PORT=9090 ./run.sh
```

### Custom Address
```bash
./run.sh --address 127.0.0.1
# Listens only on localhost
```

### With TLS
```bash
export HAL_TLS_CERT=/path/to/cert.pem
export HAL_TLS_KEY=/path/to/key.pem
./run.sh
```

### Debug Mode
```bash
DEBUG=1 ./run.sh --debug
```

---

## Next Steps

1. **Read Full Docs**: See `HAL_SERVICE_SETUP.md` for complete details
2. **Review Commands**: Check `HAL_QUICK_REFERENCE.sh` for all options
3. **Deploy**: Use `make deploy` to deploy to Raspberry Pi 5
4. **Monitor**: Use `journalctl -u hal-server -f` to watch logs

---

## Key Features You Got

✅ **Smart Conflict Detection**  
Automatically stops existing HAL instances before starting new ones

✅ **Auto-Build**  
Builds if executable is missing, skips if up-to-date

✅ **Automatic Restart**  
Service auto-restarts on failure (systemd service only)

✅ **Comprehensive Logging**  
File logs, systemd journal, color-coded console output

✅ **Easy Uninstall**  
`sudo ./install-service.sh --uninstall` removes everything

---

## Port Information

- **Default**: `0.0.0.0:50051` (listen on all interfaces, port 50051)
- **Customizable**: Use `--port` or `HAL_PORT` env var
- **gRPC Server**: Protocol Buffers + gRPC

---

## Help & Documentation

```bash
# Show script help
./run.sh --help
./install-service.sh --help

# View full docs
less HAL_SERVICE_SETUP.md
less HAL_QUICK_REFERENCE.sh
```

---

## Emergency Commands

```bash
# Kill everything and start fresh
pkill -9 hal_server
./run.sh

# Force uninstall if install-service fails
sudo systemctl stop hal-server
sudo systemctl disable hal-server
sudo rm /etc/systemd/system/hal-server.service
sudo systemctl daemon-reload
```

---

**Need more info?** Check `HAL_SERVICE_SETUP.md` for the complete documentation.

**Happy hacking!** 🚀
