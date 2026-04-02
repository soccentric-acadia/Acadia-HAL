# ACU Test Framework for NVIDIA Platforms

## Overview

The ACU Test Framework is a comprehensive testing suite designed specifically for NVIDIA Jetson platforms and other NVIDIA-based embedded systems. This framework provides a modular, extensible architecture for testing various hardware interfaces and system components commonly found in automotive and industrial applications.

### Key Features

- **Modular Library Architecture**: Organized into separate libraries for different hardware interfaces (audio, camera, GPU, PCIe, USB, etc.)
- **Cross-Platform Support**: Supports both native compilation and Docker-based cross-compilation for ARM64 architectures
- **Comprehensive Testing**: Includes unit tests, integration tests, and deployment capabilities
- **Documentation Generation**: Integrated Doxygen support for API documentation
- **Code Quality Tools**: Built-in clang-format for code formatting and cppcheck for static analysis
- **Deployment Tools**: Automated deployment to multiple NVIDIA devices via SSH/SCP

## Prerequisites

### System Requirements

- **Operating System**: Linux (Ubuntu 18.04+ recommended)
- **Compiler**: GCC 7+ or Clang 6+
- **Build Tools**:
  - CMake 3.10+
  - Ninja build system
  - Make
  - Git
- Docker installed and running

## Build Process

The project supports two primary build methods:

1. **Native Compilation**: Build directly on the host system
2. **Docker-based Cross-Compilation**: Build using Docker containers for ARM64 targets

### Native Compilation

Native compilation builds the project for the host architecture (typically x86_64).

#### Build Process

1. **Install Dependencies**
   ```bash
   sudo apt update
   sudo apt install -y build-essential cmake ninja-build git \
                       protobuf-compiler libprotobuf-dev libprotoc-dev \
                       libgrpc++-dev libsqlite3-dev libboost-system-dev \
                       libboost-date-time-dev doxygen clang-format cppcheck
   ```

2. **Build and Run Tests**
   ```bash
   make build
   make run
   ```

### Docker-based Cross-Compilation

Docker-based compilation uses NVIDIA's L4T (Linux for Tegra) base image to cross-compile for ARM64 architectures, specifically targeting NVIDIA Jetson platforms.

#### Build Process

1. **Ensure Docker is Installed**
   ```bash
   make docker-install
   ```

2. **Build the Docker Image**
   ```bash
   make docker-image
   ```

   This creates a Docker image named `jetson-builder` based on `nvcr.io/nvidia/l4t-base` with all necessary build dependencies pre-installed.

3. **Build**
   ```bash
   make docker-build
   ```

   This command:
   - Runs the Docker container with the project mounted
   - Configures the build system with Ninja generator
   - Builds all targets
   - Outputs binaries in the `build/cross/` directory

4. **Run at target**

```bash
   # For Orin Primary
   make run-a
   # For Orin Secondary
   make run-b
   # IP address specified
   make run-b IP_B=192.168.200.2
```


## Make Targets

For the full list of supported commands, run:

```bash
make help
```

Current output:

```text
Available targets:
  Native builds (host architecture):
    configure    - Configure the project with CMake/Ninja
    build        - Build the project (configures if needed)
    test         - Run tests
    run          - Run test app locally (usage: make run [name] [args...], no args = run all)

  Common (host an docker):
    clean        - Remove build directory
    list         - List available test applications

  Cross builds (Docker):
    all                  - Alias for docker-build (default)
    docker-install       - Install Docker if not present
    docker-image         - Build Docker image if not exists
    docker-configure     - Configure project with Docker/Ninja
    docker-build         - Build project with Docker/Ninja
    docker-clean         - Clean Docker build artifacts
    docker-shell         - Start Docker shell for development

  Deploy & Run (on target device):
    deploy       - Deploy to all devices (deploy-a + deploy-b)
    deploy-a     - Deploy lib and bin to 192.168.1.3
    deploy-b     - Deploy lib and bin to 192.168.1.13
    run-a        - Run test app on 192.168.1.3 (usage: make run-a [name] [args...], no args = run all)
    run-b        - Run test app on 192.168.1.13 (usage: make run-b [name] [args...], no args = run all)

  Code quality & Docs:
    format       - Format code with clang-format
    cppcheck     - Run static analysis with cppcheck
    doc          - Generate Doxygen documentation

  Misc:
    setup-completion - Set up bash completion for make (adds to ~/.bashrc)
```


## Contributing

1. Follow the established code formatting (use `make format`)
2. Run static analysis before submitting (`make cppcheck`)
3. Add tests for new functionality
4. Update documentation as needed

