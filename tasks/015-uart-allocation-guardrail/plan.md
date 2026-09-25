# Task 015 — UART allocation guardrail (VB-7) — Acadia-HAL scope

Parent task: [`Acadia/tasks/015-uart-allocation-guardrail/plan.md`](https://github.com/soccentric-acadia/Acadia/blob/develop/tasks/015-uart-allocation-guardrail/plan.md)

## Scope in this repository

`src/platform_rpi5.h`: renamed the misleadingly generic `RPI5_UART1`
(`/dev/ttyAMA10`) to `RPI5_PICO_LINK_UART`, with a comment explaining it is
the Pico W real-time co-processor link (not a general-purpose second UART)
and citing the regression history (`improvements.md` HW-004). Updated the
one consumer, `src/mcu_interface_lib/mcu_interface_lib.cpp`, to reference
the named constant instead of a bare `"/dev/ttyAMA10"` string literal.

No functional change: `rs485_lib.cpp`/`uart_lib.cpp` are unimplemented
stubs that never referenced this constant at all, so the actual VB-7 hazard
(RS-485/UART tests driving the Pico link) cannot currently occur — this is
a guardrail against it being reintroduced when those stubs are implemented.

## Local tests

Real toolchain used (after installing `protobuf-compiler`, `libgrpc++-dev`,
`protobuf-compiler-grpc`, none present by default):

```
cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=Release
ninja                                    # 397/397 targets, clean
LD_LIBRARY_PATH=build/lib ./build/bin/test_mcu_interface   # runs, same output as before
```

## Role in cross-repository validation

Self-contained. Also surfaced, while investigating `Acadia-Hardware` for
the actual pin-map values this constant should encode: that repository's
`pin-map/board_rev_a.json` does not exist and has never existed in its git
history — see the parent task's plan.md and `improvements.md` HW-019 for
the full finding. Not something this repository can fix.
