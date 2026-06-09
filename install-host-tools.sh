#!/usr/bin/env bash
# install-host-tools.sh — installs optional host-side tooling that the
# Acadia-HAL Makefile targets assume are present.
#
# Run on the x86_64 host (NOT inside the ARM64 Docker builder). Provides:
#   - clang-format       (make format)
#   - cppcheck           (make cppcheck)
#   - doxygen + graphviz (make doc)
#   - cmake-format       (make format, optional)
#   - rpm + rpm-build    (make package-rpm)
#   - qemu-user-static + binfmt-support  (make docker-build on x86_64 host)
#
# Usage:
#   chmod +x install-host-tools.sh
#   ./install-host-tools.sh
#
# Requires sudo.
set -euo pipefail

if [ "$(id -u)" -ne 0 ] && ! command -v sudo >/dev/null 2>&1; then
  echo "Error: this script needs sudo or root." >&2
  exit 1
fi

SUDO=""
if [ "$(id -u)" -ne 0 ]; then
  SUDO="sudo"
fi

echo "==> Installing clang-format, cppcheck, doxygen, graphviz, cmake-format, rpm, rpm-build"
$SUDO apt-get update
$SUDO apt-get install -y \
  clang-format \
  cppcheck \
  doxygen \
  graphviz \
  cmake-format \
  rpm \
  rpm-build

echo "==> Installing QEMU user-mode emulation + binfmt (required for 'make docker-build' on x86_64 host)"
$SUDO apt-get install -y qemu-user-static binfmt-support
$SUDO update-binfmts --enable qemu-aarch64 || true

# If docker binfmt is preferred (alternative to host binfmt), print the
# docker command as well — non-privileged container, no sudo needed.
cat <<'EOF'

Verification commands (after install):
  command -v clang-format cppcheck doxygen rpm cmake-format
  ls /proc/sys/fs/binfmt_misc/qemu-aarch64 && cat /proc/sys/fs/binfmt_misc/qemu-aarch64

  make format
  make cppcheck
  make doc
  make package-rpm
  make docker-build

EOF
