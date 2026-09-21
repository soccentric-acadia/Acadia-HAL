# Changelog

## 2026-09-21 — Root-agent feature workflow guard (task 006)

- Added a canonical instruction requiring leaf-started agents to refuse feature
  changes and direct the user to the Acadia parent root.

## 2026-09-18 — Agent documentation and task workflow standardization

- Replaced the detailed root `CLAUDE.md` with a canonical pointer and added
  `AGENTS.md`.
- Added a concise canonical document with Docker build and test instructions.
- Added ignored local `Plan.md` and `Task.md` workflow artifacts.
- Verified the required document shape and the declared `make build` and
  `make test` targets.

All notable changes to Acadia-HAL are documented here. Format: Keep a
Changelog; versioning: SemVer. HAL version is the `project()` version in
CMakeLists.txt.

## [1.0.0] — initial baseline

- Modular hardware-abstraction libraries for Raspberry Pi 5 (BCM2712 + RP1)
  with `src/platform_rpi5.h` platform constants.
- Canonical Protobuf/gRPC contract ownership (`protos/`): `common.proto`,
  `hal_service.proto`, `client_service.proto`, per-module protos.
- Docker ARM64 cross build, Doxygen, clang-format + cppcheck gates.
- Deb/IPK packaging targets; service install assets.

### Changed
- docs: corrected Acaida→Acadia typos and the malformed port-history
  blockquote (HAL-001).
- chore: gitignore generated proto/gRPC bindings — HAL owns the `.proto`
  sources; generated code must never be committed (X1-002).

[1.0.0]: https://github.com/soccentric-acadia/Acadia-HAL/releases/tag/v1.0.0
