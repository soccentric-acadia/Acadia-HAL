# ACU Test Framework for NVIDIA Platforms

## Overview

The ACU (Automotive Control Unit) Test Framework is a comprehensive testing suite designed specifically for NVIDIA Jetson platforms and other NVIDIA-based embedded systems. This framework provides a modular, extensible architecture for testing various hardware interfaces and system components commonly found in automotive and industrial applications.

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
- **Libraries**:
  - Protocol Buffers 3.21+
  - gRPC 1.54+
  - Boost (system, date-time)
  - SQLite3

### For Docker-based Cross-Compilation

- Docker installed and running
- NVIDIA Docker support (if using GPU acceleration)

## Build Process

The project supports two primary build methods:

1. **Native Compilation**: Build directly on the host system
2. **Docker-based Cross-Compilation**: Build using Docker containers for ARM64 targets

### Native Compilation

Native compilation builds the project for the host architecture (typically x86_64).

#### Step-by-Step Build Process

1. **Clone the Repository**
   ```bash
   git clone <repository-url>
   cd acu-test-framework-nvidia-platform
   ```

2. **Install Dependencies**
   ```bash
   # Ubuntu/Debian
   sudo apt update
   sudo apt install -y build-essential cmake ninja-build git \
                       protobuf-compiler libprotobuf-dev libprotoc-dev \
                       libgrpc++-dev libsqlite3-dev libboost-system-dev \
                       libboost-date-time-dev doxygen clang-format cppcheck
   ```

3. **Configure the Build**
   ```bash
   make configure
   ```

4. **Build the Project**
   ```bash
   make build
   ```

5. **Run Tests (Optional)**
   ```bash
   make test
   ```

6. **Generate Documentation (Optional)**
   ```bash
   make doc
   # Documentation will be generated in build/html/
   ```

7. **Install the Project (Optional)**
   ```bash
   make install
   ```

#### Additional Native Build Targets

- `make format`: Format code using clang-format
- `make cppcheck`: Run static analysis with cppcheck
- `make clean`: Clean build artifacts
- `make rebuild`: Quick rebuild without reconfiguring

### Docker-based Cross-Compilation

Docker-based compilation uses NVIDIA's L4T (Linux for Tegra) base image to cross-compile for ARM64 architectures, specifically targeting NVIDIA Jetson platforms.

#### Step-by-Step Docker Build Process

1. **Ensure Docker is Installed**
   ```bash
   make install-docker
   ```

2. **Build the Docker Image**
   ```bash
   make build-docker
   ```

   This creates a Docker image named `jetson-builder` based on `nvcr.io/nvidia/l4t-base:r35.1.0` with all necessary build dependencies pre-installed.

3. **Build the Project Using Docker**
   ```bash
   make docker-build-project
   ```

   This command:
   - Runs the Docker container with the project mounted
   - Configures the build system with Ninja generator
   - Builds all targets
   - Outputs binaries in the `build/` directory

#### Alternative Docker Build Methods

**Using Individual Targets:**
```bash
# Build image
make docker-build-image

# Configure project
make docker-configure

# Build project
make docker-build
```

**Interactive Docker Shell:**
```bash
make docker-shell
# This starts an interactive shell in the Docker container
# You can then run build commands manually
```

#### Docker Build Artifacts

After successful build, the following artifacts are created in the `build/` directory:
- `lib/libacu_full_test.a`: Static library
- `lib/libacu_full_test_shared.so`: Shared library
- `bin/`: Executable test applications
- `html/`: Doxygen documentation (if generated)

### Deployment

The framework includes automated deployment tools for testing on NVIDIA devices.

#### Deploy to Target Devices

```bash
make deploy
```

This deploys the built libraries and executables to predefined NVIDIA devices:
- Device A: 192.168.1.3 (user: nvidia)
- Device B: 192.168.1.13 (user: nvidia)

#### Run Tests on Target Devices

```bash
# Run on Device A
make run-a <test_application> [arguments]

# Run on Device B
make run-b <test_application> [arguments]

# Example
make run-a gpu_test --verbose
```

#### Run Tests Locally

```bash
make run-local <test_application> [arguments]
```

## Development Workflow

### Code Formatting

```bash
# Native
make format

# Docker
make docker-format
```

### Static Analysis

```bash
# Native
make cppcheck

# Docker
make docker-cppcheck
```

### Documentation

```bash
# Native
make doc

# Docker
make docker-doc
```

### Bash Completion

Set up bash completion for make targets:

```bash
make setup-completion
source ~/.bashrc
```

## Configuration

### Build Options

The Makefile handles all build configuration automatically. The project is configured to use:
- Ninja generator for fast builds
- Release build type by default
- Testing enabled by default

### Environment Variables

- `BUILD_DIR`: Build directory (default: `build`)
- `DOCKER_IMAGE`: Docker image name (default: `jetson-builder`)
- `IP_A`, `IP_B`: Target device IP addresses
- `USER`: SSH username for deployment (default: `nvidia`)

## Troubleshooting

### Common Issues

1. **Build Configuration Fails**
   - Ensure all dependencies are installed
   - Check CMake version (3.10+ required)
   - Run `make configure` to see detailed configuration output

2. **Docker Build Fails**
   - Ensure Docker daemon is running
   - Check available disk space
   - Verify NVIDIA Docker support if using GPU

3. **Cross-Compilation Issues**
   - Ensure Docker image is built with correct base image
   - Check toolchain configuration

4. **Deployment Fails**
   - Verify SSH keys are set up for target devices
   - Check network connectivity to target IPs
   - Ensure target devices have necessary permissions

### Getting Help

- Check the generated Doxygen documentation in `build/html/`
- Review build output for detailed error messages
- Use `make help` to see all available targets

## Contributing

1. Follow the established code formatting (use `make format`)
2. Run static analysis before submitting (`make cppcheck`)
3. Add tests for new functionality
4. Update documentation as needed

## License

[Specify your license here]