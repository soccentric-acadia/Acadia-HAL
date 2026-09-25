# Changelog

## 2026-09-25 — Task 015: UART allocation guardrail (VB-7)

- **Parent task:** `soccentric-acadia/Acadia` `tasks/015-uart-allocation-guardrail/`.
- **User-visible behaviour:** none — `RPI5_UART1` renamed to
  `RPI5_PICO_LINK_UART` in `src/platform_rpi5.h` (same value,
  `/dev/ttyAMA10`), with a comment warning it is the Pico W real-time
  co-processor link and must not become a default RS-485/UART test device
  (a regression that already happened once, per `improvements.md` HW-004).
  `src/mcu_interface_lib/mcu_interface_lib.cpp` updated to use the named
  constant instead of a bare string literal.
- **Validation:** full native build (397/397 targets) + `test_mcu_interface`
  run, both clean.
- **Also found (not fixed here):** `Acadia-Hardware`'s pin map
  (`pin-map/board_rev_a.json`), which several `improvements.md`/
  `integration-ivv.md` findings cite by path and line number, does not
  exist and has never existed in that repository's git history — see the
  parent task record and `improvements.md` HW-019.
