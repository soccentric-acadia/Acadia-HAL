# Task 002 (leaf) — Add PREEMPT_RT patch to the Linux kernel — Acadia-HAL

Parent task: `Acadia/tasks/002-add-preempt-rt-kernel-patch/plan.md`
- **Task ID:** 002
- **Date:** 2026-09-21
- **Status:** approved 2026-09-21 (aarch64-only scope, D1 resolved) — build-side complete; on-target pending
- **Class:** validation-only

## Scope in this repository

None (no code change). Acadia-HAL runs on the Acadia-OS image; the PREEMPT_RT
kernel changes IRQ threading and scheduling latency, so the HAL device tests
must be re-run against the RT image to confirm the hardware path still behaves
within its timing envelope.

## Affected interfaces

- HAL gRPC peripheral test flows (discovery, single-shot/monitoring/event modes)
  exercised against the RT image — protocol and schema unchanged.

## Expected validation role

1. Build/host unit tests: expected unaffected (no source change; host tests
   independent of target kernel) — may be skipped with a recorded reason.
2. On-target: run the existing HAL device tests against the RT image built by
   Acadia-OS (task 002), covering CAN, 1-Wire, PWM, and RTC paths where the lab
   hardware allows; results recorded in this leaf's `validation.md` with the
   image revision tested.

## Risk to verify

- Threaded IRQs (e.g. mcp2515 CAN interrupt on GPIO 25) may shift the
  interrupt-to-HAL-read latency; confirm peripheral operations remain within
  documented timeouts (no HAL change expected).