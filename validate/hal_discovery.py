#!/usr/bin/env python3
"""hal_discovery.py — find every executable under the configured bin dirs on
the target machine, verify its ELF architecture, and snapshot system info."""

from __future__ import annotations

from dataclasses import dataclass, field

# ELF e_machine values (offset 18, 2 bytes little-endian)
EM_AARCH64 = 0xB7
EM_X86_64 = 0x3E

DEFAULT_REMOTE_BIN_DIRS = [
    "/opt/hal/bin",
    "$HOME/acadia-hal/build/cross/bin",
    "$HOME/acadia-hal/build/native/bin",
]
DEFAULT_LOCAL_BIN_DIRS = [
    "build/bin",
]


@dataclass
class Binary:
    name: str
    path: str
    arch: str = "unknown"        # aarch64 | x86_64 | unknown
    arch_ok: bool | None = None  # None = not checked
    kind: str = "test"           # test | server

    def __post_init__(self):
        self.kind = "server" if self.name == "hal_server" else "test"


@dataclass
class SystemInfo:
    raw: dict = field(default_factory=dict)

    def render(self) -> str:
        lines = [f"{k:<24}: {v}" for k, v in self.raw.items()]
        return "\n".join(lines)


def _expand_tilde_in_cmd(cmd: str) -> str:
    # `find $HOME/...` relies on shell expansion — keep as-is; ssh runs bash -lc.
    return cmd


def discover(runner, bin_dirs: list[str]) -> tuple[list[Binary], list[str]]:
    """Return (binaries, problems). Dedup by basename, first dir wins."""
    if not bin_dirs:
        return [], ["no bin dirs configured"]
    dirs_q = " ".join("'" + d + "'" for d in bin_dirs)
    cmd = (
        f"for d in {dirs_q}; do "
        "d=$(eval echo \"$d\"); "
        "[ -d \"$d\" ] || continue; "
        "find \"$d\" -maxdepth 1 -type f -executable -printf '%p\\n' 2>/dev/null; "
        "done"
    )
    res = runner.run(cmd, timeout=30)
    if res.timed_out:
        return [], ["discovery timed out"]
    seen: dict[str, Binary] = {}
    for line in res.stdout.splitlines():
        path = line.strip()
        if not path or "/" not in path:
            continue
        name = path.rsplit("/", 1)[1]
        if name in seen:
            continue
        seen[name] = Binary(name=name, path=path)
    problems = []
    if res.rc != 0 and res.stderr.strip():
        problems.append(f"find stderr: {res.stderr.strip()[:200]}")
    return list(seen.values()), problems


def check_arch(runner, binaries: list[Binary]) -> None:
    """Use `file -b` when available, else parse ELF header via od."""
    if not binaries:
        return
    paths = " ".join("'" + b.path + "'" for b in binaries)
    # 1) try file(1)
    res = runner.run(
        "command -v file >/dev/null 2>&1 && echo yes || echo no", timeout=10)
    if res.ok and res.stdout.strip() == "yes":
        cmd = f"file -b {paths}"
        r = runner.run(cmd, timeout=60)
        lines = r.stdout.splitlines()
        if len(lines) == len(binaries):
            for b, line in zip(binaries, lines):
                if "aarch64" in line or "ARM aarch64" in line:
                    b.arch, b.arch_ok = "aarch64", True
                elif "x86-64" in line:
                    b.arch, b.arch_ok = "x86_64", True
                elif "ELF" in line:
                    b.arch, b.arch_ok = "other-elf", False
                else:
                    b.arch, b.arch_ok = "not-elf", False
            return
    # 2) fallback: read e_machine (2 bytes at offset 18, little-endian)
    for b in binaries:
        r = runner.run(
            f"od -An -tx1 -j18 -N2 '{b.path}' 2>/dev/null", timeout=10)
        if not r.ok:
            b.arch, b.arch_ok = "unknown", False
            continue
        try:
            lo, hi = (int(x, 16) for x in r.stdout.split())
            mach = lo | (hi << 8)
        except ValueError:
            b.arch, b.arch_ok = "unknown", False
            continue
        if mach == EM_AARCH64:
            b.arch, b.arch_ok = "aarch64", True
        elif mach == EM_X86_64:
            b.arch, b.arch_ok = "x86_64", True
        else:
            b.arch, b.arch_ok = f"elf:{mach:#06x}", False


def snapshot_system(runner) -> SystemInfo:
    info = SystemInfo()
    script = r"""
echo "hostname=$(hostname 2>/dev/null)"
echo "uname=$(uname -srm 2>/dev/null)"
echo "os=$( (grep PRETTY_NAME /etc/os-release 2>/dev/null || true) | cut -d'"' -f2 )"
echo "model=$(cat /proc/device-tree/model 2>/dev/null | tr -d '\0')"
echo "kernel_cmdline_serial=$(cat /proc/cmdline 2>/dev/null | head -c 120)"
echo "uptime=$(uptime -p 2>/dev/null)"
echo "cpu_count=$(nproc 2>/dev/null)"
echo "mem_total=$(free -h 2>/dev/null | awk '/^Mem:/{print $2}')"
echo "cpu_temp=$(cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null)"
echo "disk_root=$(df -h / 2>/dev/null | awk 'NR==2{print $2" total, "$4" free"}')"
echo "user=$(id -un 2>/dev/null)  uid=$(id -u 2>/dev/null)"
echo "systemd_hal_server=$(systemctl is-active hal-server 2>/dev/null || echo inactive)"
"""
    res = runner.run(script, timeout=20)
    for line in res.stdout.splitlines():
        if "=" in line:
            k, _, v = line.partition("=")
            info.raw[k.strip()] = v.strip()
    # human readable temperature
    try:
        mt = int(info.raw.get("cpu_temp", "0") or 0)
        if mt > 1000:
            info.raw["cpu_temp"] = f"{mt / 1000:.1f} C"
    except (ValueError, TypeError):
        pass
    return info