# Acadia-HAL Build and Deployment

## Scope

Acadia-HAL is the C++ hardware-abstraction and test-module library. Its root Makefile owns
native CMake/Ninja builds, ARM64 Docker cross-builds, CTest, DEB/RPM/IPK packaging, static
analysis, Doxygen, and copying test binaries/libraries to Raspberry Pi targets.

## 1. Prerequisites

- Linux, GNU Make, CMake, Ninja, a C++ compiler, and the libraries discovered by the root and
`protos/` CMake files, including protobuf/gRPC and the test dependencies used by the module
tree.
- `clang-format`, `cppcheck`, Doxygen, and optionally `cmake-format` for quality/documentation
targets.
- Docker for ARM64/QEMU builds and packaging; `make docker-install` may use `sudo` to install
Docker.
- `sshpass`, SSH/SCP access, and a reachable target for deployment targets.

## 2. Setup

```sh
make help
make configure
```

Native configuration creates `build/native` and runs `cmake -GNinja`. The repository also
contains protobuf CMake sources and module test programs; keep generated/shared contract inputs
available when configuring.

## 3. Build and compile

```sh
make build
make docker-image
make docker-configure
make docker-build
```

Native binaries and libraries are under `build/native/{bin,lib}`. ARM64 cross outputs are under
`build/cross/{bin,lib}`. `make all` aliases `docker-build`; it is conditional on Docker and
QEMU/platform support.

## 4. Test and lint

```sh
make test
make docker-test
make format
make cppcheck
make doc
```

Native and Docker tests run CTest; Docker runs the tests in the ARM64 container. `format`
requires `clang-format` and optionally `cmake-format`; `cppcheck` requires `cppcheck`; `doc`
requires Doxygen and Graphviz. These checks are command-level validation and do not prove
physical hardware behavior.

## 5. Packaging and artifacts

```sh
make package
make package-deb
make package-ipk
make package-rpm
make docker-package
make docker-package-deb
make docker-package-ipk
make docker-package-rpm
```

Native packages are generated in `build/native/`; cross packages are generated in
`build/cross/`. DEB and IPK are included by `package`; RPM additionally requires `rpmbuild` in
the environment/container. The package names printed by the Makefile use the `acadia-hal_*.deb`,
`acadia-hal_*.ipk`, and `acadia-hal-*.rpm` patterns.

## 6. Deployment, install, and run

```sh
make deploy-a
make deploy-b
make deploy IP_A=192.168.1.20 IP_B=192.168.1.21 HAL_SSH_USER=pi HAL_SSH_PASS='...'
make run-a
make run-b APP_NAME arg1
make run APP_NAME arg1
```

`deploy-*` first cross-builds and then copies `build/cross/lib` and `build/cross/bin` to
`/tmp/acu-test/` on the target. Defaults are `192.168.1.3`, `192.168.1.13`, and user/password
`nvidia`; use `HAL_SSH_USER`/`HAL_SSH_PASS` or `USER`/`PASS` overrides and do not commit
secrets. `run` uses native binaries and `LD_LIBRARY_PATH`; `run-a`/`run-b` execute on the remote
target. No cloud installation target exists.

## 7. Cleanup

```sh
make clean
make docker-clean
```

`clean` removes `build/`. `docker-clean` removes the cross build directory inside the container
but retains the image and ccache volume.

## 8. Hardware, cloud, and manual verification caveats

Physical board access, device nodes, drivers, GPIO/SPI/I2C/CAN peripherals, timing, and ABI
compatibility require manual target verification. Docker/QEMU CTest is useful for build and
contract coverage only. Before installing a DEB/IPK/RPM, confirm target architecture and
package-manager compatibility; after deployment, verify each relevant module against real
hardware and check logs, permissions, and service integration. The password-based `sshpass`
recipes also disable strict host-key checking and should be restricted to controlled development
networks.
