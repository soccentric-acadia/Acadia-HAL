# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repo is

`Acadia-HAL` is one of 7 components in the `Acadia` project (per the monorepo
topology in `~/.claude/CLAUDE.md`). It is the on-target **C++17 Hardware
Abstraction Layer** for a **Raspberry Pi 5** (BCM2712 SoC + RP1 south-bridge),
originally ported from a Jetson Orin baseline. The global CLAUDE.md covers
project-wide rules (architecture, interface contracts, workflows). This file
covers **only what is local to this repo**.

## Build, test, package

All commands run from the repo root.

### Native (host) — fastest iteration
```bash
make configure      # cmake -GNinja → build/native
make build          # ninja -j$(nproc) → build/native/{bin,lib}
make test           # ctest --output-on-failure
make run            # runs every test binary locally; or `make run <name> [args]`
make list           # show available test binaries
```
Native build uses `build/native/`. Test binaries land in `build/native/bin/`,
shared lib in `build/native/lib/`. Run with `LD_LIBRARY_PATH=build/native/lib`.

### Cross (ARM64) — the real target
```bash
make docker-image         # builds the `acadia` ARM64 Ubuntu 22.04 builder
make docker-build         # configure + build in Docker; outputs build/cross/
make docker-test          # ctest inside the ARM64 container (QEMU)
make docker-shell         # interactive shell in the builder
```

### Deploy to a real Pi 5
```bash
make run-a                # deploys build/cross to IP_A=192.168.1.3 and runs all bins
make run-b                # same for IP_B=192.168.1.13
make run-a test_cpu long-term 1000 30s   # one binary, with args
make run-b IP_B=192.168.200.2             # override IP from CLI
make deploy USER=pi HAL_SSH_PASS=...      # override creds (never commit)
```

### Packaging (DEB / RPM / IPK)
```bash
make package              # DEB + IPK from the native build
make docker-package       # DEB + IPK from the cross (ARM64) build
make package-rpm          # needs `rpm` installed locally
make docker-package-deb docker-package-rpm docker-package-ipk   # granular
```
The IPK is the DEB repackaged with a `.ipk` extension
(`cmake/rename_deb_to_ipk.cmake`); the on-disk format is identical.

### Code quality & docs
```bash
make format               # clang-format -i; cmake-format if available
make cppcheck             # static analysis
make doc                  # Doxygen (config: Doxyfile.in)
```

### Single-test invocation
Each test binary accepts the same CLI via `test_runner.h`:
```bash
./test_cpu one-shot                              # default
./test_cpu monitoring 1000                       # loop, Ctrl+C to stop
./test_cpu long-term 1000 30s                    # interval 1s, run 30s
./test_cpu monitor-test 1000 30s                 # long-term + Pi5 hw monitors
./test_cpu event 500 any-fail 5m                 # fire on first failure, 5min cap
./test_cpu event 1000 fail-rate-above-10         # >10% failures fires event
./test_cpu --help                                # all modes
```

## High-level architecture

Three layers, built top-down in this order by the root `CMakeLists.txt`:

```
protos/                  ← gRPC + protobuf layer (builds hal_protos)
   ├── common.proto        shared types: Status, Severity, TypedValue, TestResult,
   │                       MonitoringSnapshot, ResultEnvelope, ExecutionMode
   ├── hal_service.proto   ONE stable gRPC service: HalService {…}
   ├── modules/<name>.proto  one per HAL module (50 modules)
   ├── registry/modules.yaml  machine-readable module manifest
   ├── server/              gRPC service impl (hal_protos_server)
   └── client/              gRPC client wrapper (hal_protos_client)

src/                     ← HAL implementation (builds acu_full_test + lib_base)
   ├── lib_base.{h,cpp}     TestBase + canonical hal::{TestResult, TestOutput,
   │                       Status, Severity, Value, MonitorItem, EventCriteria}
   ├── test_runner.h        runTestApp<T>() template — every main.cpp uses it
   ├── platform_rpi5.h      ALL Pi 5 hardware constants (paths, freq, temps, …)
   └── <module>_lib/        one folder per module; contains:
                            <module>_lib.h (class : public TestBase)
                            <module>_lib.cpp
                            CMakeLists.txt (defines `<module>_lib` target)

tests/                   ← one CLI binary per module (builds via tests/<name>/)
   └── <name>/
        CMakeLists.txt       add_executable(test_<name> main.cpp)
        main.cpp             `return runTestApp<X>("X", argc, argv);`
```

### Canonical result types (`src/lib_base.h`, mirrored in `protos/common.proto`)

```cpp
namespace hal {
  using Value = std::variant<bool, int64_t, double, std::string>;
  enum class Status   { PASS, FAIL, ERROR, TIMEOUT, SKIPPED, ABORTED };
  enum class Severity { INFO, WARNING, ERROR, CRITICAL };
  struct TestOutput { Status status; Severity severity;
                      map<string,string> reason;
                      map<string,Value>  metrics, monitoring, event;
                      string             error_message; };
  struct TestResult { string module_id, test_id, name; uint64 sequence;
                      chrono::system_clock::time_point timestamp;
                      chrono::nanoseconds duration;
                      TestOutput output; vector<TestResult> subtests; };
}
```
Older module protos (cpu, gpu, …) embed `hal.common.TestResult`; newer ones
(i2c, spi, uart, rtc, …) use a flatter `Result { status, message, TestDetail[] }`
shape. Both are valid; the manifest in `protos/registry/modules.yaml` is the
source of truth for which proto a given `module_id` resolves to.

### Four execution modes (TestBase, mirrored in `protos/common.proto::ExecutionMode`)

| `TestMode`     | RPC              | Behaviour                                         |
|----------------|------------------|---------------------------------------------------|
| `SINGLE_SHOT`  | `RunSingleShot`  | One pass, return one `ResultEnvelope`             |
| `LONG_TERM`    | `StartLongTerm`  | Cycle at interval_ms for duration_ms; stream per cycle |
| `MONITORING`   | `StartMonitoring`| Cycle forever, sample `MonitorItem`s, stream per cycle |
| `EVENT_BASED`  | `StartEventBased`| Poll forever, stream only when `EventCriteria` fires |

`TestBase::runTests/Monitoring/LongTerm/EventTest` accept a callback.
Background loops honour `stop()`, `pause()`, `resume()`, and `isRunning()`.

## How to add a new module

1. Create `src/<id>_lib/<id>_lib.{h,cpp}` — class `<X>Testing : public TestBase`
   implementing `runAllTests()`, `runSingleTest(name)`, `getAvailableTests()`.
2. Create `src/<id>_lib/CMakeLists.txt` defining an `<id>_lib` target that links
   to `lib_base` (look at `src/cpu_lib/CMakeLists.txt` for the pattern).
3. Add `add_subdirectory(<id>_lib)` to `src/CMakeLists.txt`.
4. Create `tests/<id>/{CMakeLists.txt,main.cpp}` using
   `runTestApp<<X>Testing>`. Add `add_subdirectory(<id>)` to `tests/CMakeLists.txt`.
5. Create `protos/modules/<id>.proto` (minimum: `Config { test_names }` +
   `Result { … }`). See `protos/README.md`.
6. Add one entry to `protos/registry/modules.yaml`.
7. Re-run CMake — `protos/CMakeLists.txt` globs `modules/*.proto`, no
   `hal_service.proto` or `common.proto` changes needed.

**Constraint:** Never modify `hal_service.proto` or `common.proto` to add a
module — the service contract is permanent. New types go in the module proto
and travel over `google.protobuf.Any`.

## Hard rules — local to this repo

These supplement the global CLAUDE.md:

- **`hal_service.proto` and `common.proto` field numbers are wire-format
  permanent.** Never renumber. When removing a field, add
  `reserved <num>; reserved "<old_name>";` (see `common.proto::TestOutput` for
  the pattern). See `protos/README.md` for the full numbering rules.
- **Module `id` strings in `registry/modules.yaml` are permanent** — never
  reuse a removed id. Bump the `version` integer on any contract change.
- **Platform constants live in `src/platform_rpi5.h`.** Do not hardcode
  Pi 5 device paths, frequencies, or temps inside module code — import
  from `platform_rpi5.h`. (Jetson-era hardcoded constants are a known
  migration smell.)
- **Every test binary's `main.cpp` must be a 5-liner** that calls
  `runTestApp<T>`. Do not add custom CLI parsing in module test mains —
  extend `test_runner.h` instead.
- **Use `LD_LIBRARY_PATH=build/native/lib` when running native binaries
  directly.** The Makefile's `run` target does this; bare `./test_cpu` will
  not find `libacu_full_test_shared.so`.
- **Generated proto sources land in `<build>/protos/generated/`** and are
  never committed. Do not add `*.pb.cc` / `*.pb.h` files to the repo.
- **The refactor of the result type is complete** (per `hal_refactor/`).
  `TestResult::output.passed` and `::data` are gone — use
  `output.status` and the typed `metrics`/`monitoring`/`event` maps.
  See `hal_refactor/09_final_report.md` for the audit trail.

## Layout quick reference

```
Makefile                        orchestrates native + Docker builds, deploy, packaging
CMakeLists.txt                  top-level: protos → src → tests; CPack
protos/                         protobuf + gRPC layer
  common.proto                  shared types (Status, Severity, TestResult, …)
  hal_service.proto             single stable gRPC service contract
  modules/<id>.proto            one per module
  registry/modules.yaml         manifest (id → proto types, version, status)
  server/                       gRPC service impl
  client/                       gRPC client wrapper
src/                            HAL C++17 implementation
  lib_base.{h,cpp}              TestBase + canonical result types
  test_runner.h                 runTestApp<T>() template
  platform_rpi5.h               all Pi 5 hardware constants
  <module>_lib/                 one folder per module
tests/                          one CLI binary per module, uses test_runner.h
docker/                         ARM64 builder image (Dockerfile + Makefile)
cmake/                          CPack helpers (rename_deb_to_ipk.cmake)
hal_refactor/                   audit reports from the result-type refactor
```

## Common gotchas

- `make build` configures **and** builds in one go; `make configure` is
  separate. The cross path (`make docker-build`) is also configure+build.
- `cmake-build-debug/` exists at the repo root (an older in-source build
  artefact from before the Makefile was introduced). The current build
  tree is `build/native/`. The `cmake-build-debug/` directory is covered
  by `.gitignore`; do not commit it.
- Two different yaml module registry shapes co-exist in
  `protos/registry/modules.yaml`: the older entries (audio_interfaces
  through vision_accelerator) use `proto_package` + `config_type` +
  `result_type` + `source_path`; the newer entries (i2c, spi, uart, …)
  use `config_type_url` + `result_type_url` + a `modes` list. When
  editing, match the style of the surrounding entries.
- The `make help` target is the canonical reference for available targets
  and their env-var overrides (`HAL_SSH_USER`, `HAL_TLS_*`, `JOBS`).
- Default `make deploy` credentials are placeholders (`nvidia`/`nvidia`) —
  override with `HAL_SSH_USER` / `HAL_SSH_PASS` or on the command line.
  Never commit real credentials.
- **Cross-build via Docker requires QEMU user-mode emulation on the host**
  (x86_64 host running an ARM64 image). The `acadia` image is ARM64; without
  `qemu-user-static` + `binfmt-support` registered for aarch64, the
  container's tini (PID 1 via `--init`) fails with `Exec format error`.
  Install: `sudo apt install -y qemu-user-static binfmt-support &&
  sudo update-binfmts --enable qemu-aarch64`.
- **`make format` / `make cppcheck` / `make doc` require the matching tool**
  installed locally. They now fail fast with a one-line `Install with: sudo
  apt install -y <pkg>` message — see the `check-*` phony targets in the
  Makefile. The CMake `add_custom_target` for these is only registered when
  the tool is found at configure time, so without an upfront check the
  Makefile chains into a `ninja: error: unknown target` failure.

## Lessons Learned (Do / Don't)

**Do**
- Before running `make format`, `make cppcheck`, or `make doc`, verify the
  tool exists: `command -v clang-format cppcheck doxygen`. These targets
  silently succeed at the cmake configure step (the tool check is at the
  *ninja* level, not the cmake level), then fail with a confusing
  "unknown target" error if the tool is missing.
- The native build (`make build` → `build/native/`) is the fast feedback
  loop. All 36 ctest tests pass on the x86_64 host (35 module tests +
  `test_sysfs_helpers` regression for the `hal::sysfs` helpers in
  `lib_base.h`). Use it for iterative work; use the Docker cross-build
  only when the change is platform-specific (e.g. new `RPI5_*` constants
  in `platform_rpi5.h`).
- For the cross-build on a x86_64 host, install QEMU user-mode emulation
  once per host: `sudo apt install -y qemu-user-static binfmt-support &&
  sudo update-binfmts --enable qemu-aarch64`. Without it, `make
  docker-build` fails immediately with `exec bash failed: Exec format
  error` from tini (PID 1).
- `make package` (native) produces both `acadia-hal_1.0_amd64.deb` and the
  `.ipk` repackaging in a single invocation. The `.ipk` is a byte-for-byte
  copy of the `.deb` with the extension renamed — see
  `cmake/rename_deb_to_ipk.cmake`.
  - if tools are not install you need sudo acess do not install wite a install.sh script and ask user to run it
- For sudo-less hosts, register QEMU aarch64 with the docker-binfmt
  image: `docker run --privileged --rm tonistiigi/binfmt --install arm64`.
  This writes `/proc/sys/fs/binfmt_misc/qemu-aarch64` and works without
  `apt install qemu-user-static`. `make docker-build` then succeeds
  (verified Jun 2026).
- `make docker-package` runs `docker-package-deb` and
  `docker-package-ipk` against the cross build; produces
  `acadia-hal_1.0_arm64.deb` and `acadia-hal_1.0_arm64.ipk` in
  `build/cross/`. `file build/cross/bin/test_cpu` confirms
  `ARM aarch64` and `build/cross/acadia-hal_1.0_arm64.deb` confirms
  `Debian binary package (format 2.0)`.
- All 36 ctest tests pass under QEMU in the cross build (slowest is
  `test_thermal_management` at ~41s; total wall-clock ~41s).
- For issue #43 (sysfs helpers in 30+ module .cpp files), the minimal
  acceptable fix is: add `hal::sysfs::readSysfsLine/Int` to `lib_base.h`
  + a `tests/sysfs_helpers/` regression that wires into ctest. Do NOT
  migrate the 30+ call sites in the same PR — that's a sweep across
  many modules and a §8 'no drive-by edits' violation. The helpers
  get adopted opportunistically as each module is touched for other
  reasons.

**Don't**
- Don't run `make format` / `make cppcheck` / `make doc` without first
  confirming the tool is installed. The fail-fast check was added in the
  Makefile (May 2026) precisely to replace the original confusing
  "Configuring... ninja: unknown target" failure mode — preserve that
  check when editing the Makefile.
- Don't try `make docker-build` on a fresh x86_64 host without verifying
  QEMU aarch64 binfmt registration. The error is opaque (Exec format
  error from tini) and not the Makefile's fault.
- Don't manually `cd build/native && cmake …` to "reconfigure" — that
  creates a competing build tree alongside the Makefile-managed one and
  confuses the next `make build`. Use `make configure` or `rm -rf
  build/native && make build`.
