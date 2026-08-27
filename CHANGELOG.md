# Changelog

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
