# Master Makefile for ACU Full Test Project
# Supports both native and Docker-based builds with Ninja

.PHONY: all build configure clean format cppcheck doc test install help
.PHONY: docker-build docker-shell docker-clean docker-format docker-cppcheck docker-doc
.PHONY: install-docker build-docker docker-build-project deploy run-a run-b run-local list setup-completion
.PHONY: install-docker build-docker docker-build-project

# Build directory
BUILD_DIR := build
DOCKER_IMAGE := jetson-builder
DOCKER_BUILD_DIR := /workspace/build

# IP addresses for NVIDIA devices
IP_A := 192.168.1.3
IP_B := 192.168.1.13

# User for SSH/SCP
USER := nvidia

# Default target
all: docker-build-project

# Native build targets (existing)
configure:
	@echo "Configuring project with Ninja..."
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -GNinja ..

build: configure
	@echo "Building project with Ninja..."
	cd $(BUILD_DIR) && ninja

clean:
	@echo "Cleaning build directory..."
	rm -rf $(BUILD_DIR)

format: configure
	@echo "Formatting code..."
	cd $(BUILD_DIR) && ninja format

cppcheck: configure
	@echo "Running cppcheck static analysis..."
	cd $(BUILD_DIR) && ninja cppcheck

doc: configure
	@echo "Generating documentation..."
	cd $(BUILD_DIR) && ninja doc_doxygen

test: configure
	@echo "Running tests..."
	cd $(BUILD_DIR) && ninja test

install: build
	@echo "Installing project..."
	cd $(BUILD_DIR) && ninja install

deploy:
	@echo "Deploying to NVIDIA devices..."
	ssh $(USER)@$(IP_A) "mkdir -p /tmp/acu-test"
	scp -r $(BUILD_DIR)/lib $(USER)@$(IP_A):/tmp/acu-test/
	scp -r $(BUILD_DIR)/bin $(USER)@$(IP_A):/tmp/acu-test/
	ssh $(USER)@$(IP_B) "mkdir -p /tmp/acu-test"
	scp -r $(BUILD_DIR)/lib $(USER)@$(IP_B):/tmp/acu-test/
	scp -r $(BUILD_DIR)/bin $(USER)@$(IP_B):/tmp/acu-test/

run-a:
	@APP="$(word 2, $(MAKECMDGOALS))"; \
	ARGS="$(wordlist 3, $(words $(MAKECMDGOALS)), $(MAKECMDGOALS))"; \
	if [ -z "$$APP" ]; then \
		echo "Usage: make run-a <test_app_name> [args...]"; \
		exit 1; \
	fi; \
	echo "Running $$APP on $(IP_A) with args: $$ARGS"; \
	ssh $(USER)@$(IP_A) "cd /tmp/acu-test && ./bin/$$APP $$ARGS"

run-b:
	@APP="$(word 2, $(MAKECMDGOALS))"; \
	ARGS="$(wordlist 3, $(words $(MAKECMDGOALS)), $(MAKECMDGOALS))"; \
	if [ -z "$$APP" ]; then \
		echo "Usage: make run-b <test_app_name> [args...]"; \
		exit 1; \
	fi; \
	echo "Running $$APP on $(IP_B) with args: $$ARGS"; \
	ssh $(USER)@$(IP_B) "cd /tmp/acu-test && ./bin/$$APP $$ARGS"

run-local:
	@APP="$(word 2, $(MAKECMDGOALS))"; \
	ARGS="$(wordlist 3, $(words $(MAKECMDGOALS)), $(MAKECMDGOALS))"; \
	if [ -z "$$APP" ]; then \
		echo "Usage: make run-local <test_app_name> [args...]"; \
		exit 1; \
	fi; \
	echo "Running $$APP locally with args: $$ARGS"; \
	./$(BUILD_DIR)/bin/$$APP $$ARGS

rebuild:
	@echo "Rebuilding project..."
	cd $(BUILD_DIR) && ninja

setup-completion:
	@echo "Setting up bash completion for make..."
	@if ! grep -q "source $(PWD)/make_completion.bash" ~/.bashrc; then \
		echo "source $(PWD)/make_completion.bash" >> ~/.bashrc; \
		echo "Added completion to ~/.bashrc. Restart your shell or run 'source ~/.bashrc' to activate."; \
	else \
		echo "Completion already set up in ~/.bashrc."; \
	fi

# Docker build targets
docker-build-image:
	@echo "Building Docker image for aarch64 cross-compilation..."
	docker build --platform linux/arm64 --build-arg UID=$(shell id -u) --build-arg GID=$(shell id -g) -t $(DOCKER_IMAGE) docker/

docker-configure: docker-build-image
	@echo "Configuring project with Docker/Ninja for aarch64..."
	docker run --rm -v $(PWD):/workspace $(DOCKER_IMAGE) \
		bash -c "mkdir -p $(DOCKER_BUILD_DIR) && cd $(DOCKER_BUILD_DIR) && cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=/workspace/toolchain-aarch64.cmake .."

docker-build: docker-configure
	@echo "Building project with Docker/Ninja for aarch64..."
	docker run --rm -v $(PWD):/workspace $(DOCKER_IMAGE) \
		bash -c "cd $(DOCKER_BUILD_DIR) && ninja"

docker-clean:
	@echo "Cleaning Docker build directory..."
	docker run --rm -v $(PWD):/workspace $(DOCKER_IMAGE) \
		bash -c "rm -rf $(DOCKER_BUILD_DIR)"

docker-format: docker-configure
	@echo "Formatting code with Docker..."
	docker run --rm -v $(PWD):/workspace $(DOCKER_IMAGE) \
		bash -c "cd $(DOCKER_BUILD_DIR) && ninja format"

docker-cppcheck: docker-configure
	@echo "Running cppcheck with Docker..."
	docker run --rm -v $(PWD):/workspace $(DOCKER_IMAGE) \
		bash -c "cd $(DOCKER_BUILD_DIR) && ninja cppcheck"

docker-doc: docker-configure
	@echo "Generating documentation with Docker..."
	docker run --rm -v $(PWD):/workspace $(DOCKER_IMAGE) \
		bash -c "cd $(DOCKER_BUILD_DIR) && ninja doc_doxygen"

docker-shell: docker-build-image
	@echo "Starting Docker shell for development..."
	docker run -it --rm -v $(PWD):/workspace $(DOCKER_IMAGE) bash

docker-all: docker-build-image docker-build

# New Docker targets using the docker folder
install-docker:
	@if ! command -v docker > /dev/null 2>&1; then \
		echo "Docker not found. Installing Docker..."; \
		sudo apt update && sudo apt install -y docker.io; \
	else \
		echo "Docker is already installed."; \
	fi

build-docker: install-docker
	@if ! docker images | grep -q $(DOCKER_IMAGE); then \
		echo "Building Docker image $(DOCKER_IMAGE)..."; \
		make -C docker build; \
	else \
		echo "Docker image $(DOCKER_IMAGE) already exists."; \
	fi

docker-build-project: build-docker
	@echo "Building project using Docker..."
	docker run --rm -v $(PWD):/workspace $(DOCKER_IMAGE) bash -c "rm -rf $(DOCKER_BUILD_DIR) && mkdir -p $(DOCKER_BUILD_DIR) && cd $(DOCKER_BUILD_DIR) && cmake -GNinja .. && ninja"
# Show available targets
help:
	@echo "Available targets:"
	@echo "  Native builds (host architecture):"
	@echo "    all          - Configure and build the project (default)"
	@echo "    build        - Build the project (configures if needed)"
	@echo "    configure    - Configure the project with CMake/Ninja"
	@echo "    clean        - Remove build directory"
	@echo "    format       - Format code with clang-format"
	@echo "    cppcheck     - Run static analysis with cppcheck"
	@echo "    doc          - Generate Doxygen documentation"
	@echo "    test         - Run tests"
	@echo "    install      - Install the project"
	@echo "    deploy       - Deploy lib and bin to NVIDIA devices"
	@echo "    run-a        - Run test app on $(IP_A) (usage: make run-a <name> [args...])"
	@echo "    run-b        - Run test app on $(IP_B) (usage: make run-b <name> [args...])"
	@echo "    run-local    - Run test app locally (usage: make run-local <name> [args...])"
	@echo "    list         - List available test applications"
	@echo "    rebuild      - Quick rebuild (assumes configured)"
	@echo "    rebuild-all  - Clean and rebuild everything"
	@echo "    setup-completion - Set up bash completion for make (adds to ~/.bashrc)"
	@echo ""
	@echo "  Docker builds (aarch64 cross-compilation):"
	@echo "    docker-build-image - Build Docker image"
	@echo "    docker-configure   - Configure with Docker for aarch64"
	@echo "    docker-build       - Build with Docker for aarch64"
	@echo "    docker-clean       - Clean Docker build artifacts"
	@echo "    docker-format      - Format code with Docker"
	@echo "    docker-cppcheck    - Run cppcheck with Docker"
	@echo "    docker-doc         - Generate docs with Docker"
	@echo "    docker-shell       - Start Docker shell for development"
	@echo "    docker-all         - Build Docker image and project"
	@echo ""
	@echo "  Docker builds using docker folder (Jetson):"
	@echo "    install-docker      - Install Docker if not present"
	@echo "    build-docker        - Build Docker image if missing"
	@echo "    docker-build-project - Build project using Docker"
