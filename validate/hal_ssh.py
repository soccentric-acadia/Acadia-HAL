#!/usr/bin/env python3
"""hal_ssh.py — command execution layer for the Acadia HAL binary validator.

Two modes:
  * ssh   : run commands on the Raspberry Pi via the `ssh` binary (laptop -> Pi)
  * local : run commands directly on this machine (harness self-test / x86 build)

Zero third-party dependencies (stdlib only). All remote commands go through a
single `run(cmd, timeout)` entry point returning an ExecResult.
"""

from __future__ import annotations

import shlex
import subprocess
import time
from dataclasses import dataclass, field


@dataclass
class ExecResult:
    cmd: str
    rc: int                      # 124 == our timeout marker (like GNU timeout)
    stdout: str
    stderr: str
    duration_s: float
    timed_out: bool = False

    @property
    def ok(self) -> bool:
        return self.rc == 0 and not self.timed_out


class CommandRunner:
    """Runs commands locally or over SSH with a uniform interface."""

    def __init__(self, mode: str = "local", host: str = "", port: int = 22,
                 connect_timeout: float = 15.0, ssh_key: str = ""):
        if mode not in ("ssh", "local"):
            raise ValueError(f"unknown mode: {mode}")
        self.mode = mode
        self.host = host
        self.port = port
        self.connect_timeout = connect_timeout
        self.ssh_key = ssh_key
        self._base: list[str] = []

    # ------------------------------------------------------------------ setup
    def build_base_cmd(self) -> None:
        """Pre-build the ssh argv (done once; validated by ping())."""
        if self.mode != "ssh":
            return
        if not self.host:
            raise ValueError("ssh mode requires --host (e.g. pi@192.168.1.42)")
        self._base = ["ssh",
                      "-o", "BatchMode=yes",
                      "-o", "StrictHostKeyChecking=accept-new",
                      "-o", f"ConnectTimeout={int(self.connect_timeout)}",
                      "-o", "ServerAliveInterval=10",
                      "-p", str(self.port)]
        if self.ssh_key:
            self._base += ["-i", self.ssh_key]
        self._base += [self.host, "--"]

    # -------------------------------------------------------------------- exec
    def run(self, cmd: str, timeout: float = 60.0,
            env_export: dict | None = None) -> ExecResult:
        """Execute `cmd` (POSIX shell) and capture everything.

        `env_export` values are shell-quoted and exported before the command —
        needed because we cannot rely on a login shell over ssh.
        """
        full = cmd
        if env_export:
            exports = " ".join(f"export {k}={shlex.quote(str(v))};"
                               for k, v in env_export.items())
            full = f"{exports} {cmd}"

        start = time.monotonic()
        try:
            if self.mode == "ssh":
                proc = subprocess.run(
                    self._base + ["bash", "-lc", full],
                    capture_output=True, text=True, timeout=timeout)
            else:
                proc = subprocess.run(
                    ["bash", "-lc", full],
                    capture_output=True, text=True, timeout=timeout)
            return ExecResult(cmd, proc.returncode, proc.stdout, proc.stderr,
                              time.monotonic() - start)
        except subprocess.TimeoutExpired as exc:
            out = exc.stdout.decode() if isinstance(exc.stdout, bytes) else (exc.stdout or "")
            err = exc.stderr.decode() if isinstance(exc.stderr, bytes) else (exc.stderr or "")
            return ExecResult(cmd, 124, out or "", err or "",
                              time.monotonic() - start, timed_out=True)
        except FileNotFoundError as exc:        # ssh binary missing, etc.
            return ExecResult(cmd, 127, "", str(exc), time.monotonic() - start)

    # ----------------------------------------------------------------- helpers
    def ping(self) -> ExecResult:
        """Cheap connectivity + auth check."""
        self.build_base_cmd()
        return self.run("echo ok", timeout=self.connect_timeout + 10)

    def shell_quote(self, s: str) -> str:
        return shlex.quote(s)

    def describe(self) -> str:
        return "local" if self.mode == "local" else f"ssh://{self.host}:{self.port}"