# Task 010 — Client hardware access privileges (VB-1) — Acadia-HAL scope (validation-only)

Parent task: [`Acadia/tasks/010-hal-hardware-access-privileges/plan.md`](https://github.com/soccentric-acadia/Acadia/blob/develop/tasks/010-hal-hardware-access-privileges/plan.md)

## Scope in this repository

Validation-only. No code change. This repo's device-touching test libraries
(`i2c_lib`, `spi_lib`, `gpio_lib` equivalents, `uart_lib`, `can`-adjacent
tests, etc. — see `src/*_lib/*.cpp` opening `/dev/i2c-*`, `/dev/spidev*`,
`/dev/gpiochip*`, `/dev/tty*`) are the functional oracle for the `meta-acadia`
+ `Acadia-Client` privilege fix: once the Client process (which embeds this
HAL in-process) has the new group membership and capabilities, these tests
should be able to reach the device nodes they always intended to.

## Role in cross-repository validation

No physical Raspberry Pi 5 / HIL rig is available in this session. This
repo's role is recorded as a known follow-up: on real hardware, running the
HAL test suite through the fixed `acadia-client.service` unit and confirming
`i2c`/`spi`/`gpio`/UART/CAN tests no longer fail with `EACCES`/`EPERM` is the
actual acceptance test for VB-1. That run is out of scope for this session
(no HIL rig; see `integration-ivv.md` V-G02/HW-014) and is left as an
explicit gap in the parent `validation.md`.
