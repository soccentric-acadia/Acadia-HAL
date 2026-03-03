# Master Makefile for ACU Full Test Project
# Supports both native and Docker-based builds with Ninja

.PHONY: all build configure clean format cppcheck doc test help
.PHONY: docker-build docker-configure docker-clean docker-shell docker-install docker-image
.PHONY: deploy deploy-a deploy-b run-a run-b list setup-completion

# Build directory
BUILD_DIR := build
DOCKER_IMAGE := jetson-builder
DOCKER_BUILD_DIR := /workspace/build

# IP addresses for NVIDIA devices
IP_A := 192.168.1.3
IP_B := 192.168.1.13

# User/Password for SSH/SCP
USER := nvidia
PASS := nvidia
SSHPASS := sshpass -p $(PASS)
SSH := $(SSHPASS) ssh -o StrictHostKeyChecking=no
SCP := $(SSHPASS) scp -o StrictHostKeyChecking=no

# Default target
all: docker-build

# =============================================================================
# Native builds (host architecture)
# =============================================================================

configure:
	@echo "Configuring project with Ninja..."
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -GNinja ..

build: configure
	@echo "Building project with Ninja..."
	cd $(BUILD_DIR) && ninja

test: build
	@echo "Running tests..."
	cd $(BUILD_DIR) && ninja test

clean:
	@echo "Cleaning build directory..."
	rm -rf $(BUILD_DIR)

# =============================================================================
# Cross builds (Docker)
# =============================================================================

# -t: allocate a pseudo-TTY so Ctrl-C (SIGINT) is forwarded to the container
# --init: use tini as PID 1 to properly propagate signals to child processes (e.g. ninja)
# ccache volume: persist compilation cache across docker runs
DOCKER_RUN := docker run --rm -t --init --platform linux/arm64 -v $(PWD):/workspace -v ccache-acu:/home/jetson/.ccache $(DOCKER_IMAGE) bash -c

docker-install:
	@if ! command -v docker > /dev/null 2>&1; then \
		echo "Docker not found. Installing Docker..."; \
		sudo apt update && sudo apt install -y docker.io; \
	else \
		echo "Docker is already installed."; \
	fi

docker-image: docker-install
	@if ! docker images | grep -q $(DOCKER_IMAGE); then \
		echo "Building Docker image $(DOCKER_IMAGE)..."; \
		make -C docker build; \
	else \
		echo "Docker image $(DOCKER_IMAGE) already exists."; \
	fi

docker-configure: docker-image
	@echo "Configuring project with Docker/Ninja..."
	$(DOCKER_RUN) "mkdir -p $(DOCKER_BUILD_DIR) && cd $(DOCKER_BUILD_DIR) && cmake -GNinja .."

docker-build: docker-image
	@echo "Building project with Docker/Ninja..."
	$(DOCKER_RUN) "if [ ! -f $(DOCKER_BUILD_DIR)/build.ninja ]; then mkdir -p $(DOCKER_BUILD_DIR) && cd $(DOCKER_BUILD_DIR) && cmake -GNinja ..; fi && cd $(DOCKER_BUILD_DIR) && ninja"

docker-clean: docker-image
	@echo "Cleaning Docker build directory..."
	$(DOCKER_RUN) "rm -rf $(DOCKER_BUILD_DIR)"

docker-shell: docker-image
	@echo "Starting Docker shell for development..."
	docker run -it --rm --platform linux/arm64 -v $(PWD):/workspace -v ccache-acu:/home/jetson/.ccache $(DOCKER_IMAGE) bash

# =============================================================================
# Deploy & Run (on target device)
# =============================================================================

deploy: deploy-a deploy-b

deploy-a: docker-build
	@echo "Deploying to $(IP_A)..."
	$(SSH) $(USER)@$(IP_A) "mkdir -p /tmp/acu-test"
	$(SCP) -r $(BUILD_DIR)/lib $(USER)@$(IP_A):/tmp/acu-test/
	$(SCP) -r $(BUILD_DIR)/bin $(USER)@$(IP_A):/tmp/acu-test/

deploy-b: docker-build
	@echo "Deploying to $(IP_B)..."
	$(SSH) $(USER)@$(IP_B) "mkdir -p /tmp/acu-test"
	$(SCP) -r $(BUILD_DIR)/lib $(USER)@$(IP_B):/tmp/acu-test/
	$(SCP) -r $(BUILD_DIR)/bin $(USER)@$(IP_B):/tmp/acu-test/

run-a: deploy-a
	@APP="$(word 2, $(MAKECMDGOALS))"; \
	ARGS="$(wordlist 3, $(words $(MAKECMDGOALS)), $(MAKECMDGOALS))"; \
	if [ -z "$$APP" ]; then \
		echo "Running all test applications on $(IP_A)..."; \
		for name in $$($(SSH) $(USER)@$(IP_A) "ls /tmp/acu-test/bin/"); do \
			echo "--- Running $$name ---"; \
			$(SSH) $(USER)@$(IP_A) "cd /tmp/acu-test && ./bin/$$name" || true; \
		done; \
	else \
		echo "Running $$APP on $(IP_A) with args: $$ARGS"; \
		$(SSH) $(USER)@$(IP_A) "cd /tmp/acu-test && ./bin/$$APP $$ARGS"; \
	fi

run-b: deploy-b
	@APP="$(word 2, $(MAKECMDGOALS))"; \
	ARGS="$(wordlist 3, $(words $(MAKECMDGOALS)), $(MAKECMDGOALS))"; \
	if [ -z "$$APP" ]; then \
		echo "Running all test applications on $(IP_B)..."; \
		for name in $$($(SSH) $(USER)@$(IP_B) "ls /tmp/acu-test/bin/"); do \
			echo "--- Running $$name ---"; \
			$(SSH) $(USER)@$(IP_B) "cd /tmp/acu-test && ./bin/$$name" || true; \
		done; \
	else \
		echo "Running $$APP on $(IP_B) with args: $$ARGS"; \
		$(SSH) $(USER)@$(IP_B) "cd /tmp/acu-test && ./bin/$$APP $$ARGS"; \
	fi

list:
	@echo "Available test applications:"
	@ls -1 $(BUILD_DIR)/bin/ 2>/dev/null | sed 's/^/    /' || echo "  (none - run 'make docker-build' first)"

# =============================================================================
# Code quality & Docs
# =============================================================================

format: configure
	@echo "Formatting code..."
	cd $(BUILD_DIR) && ninja format

cppcheck: configure
	@echo "Running cppcheck static analysis..."
	cd $(BUILD_DIR) && ninja cppcheck

doc: configure
	@echo "Generating documentation..."
	cd $(BUILD_DIR) && ninja doc_doxygen

# =============================================================================
# Misc
# =============================================================================

setup-completion:
	@echo "Setting up bash completion for make..."
	@if ! grep -q "source $(PWD)/make_completion.bash" ~/.bashrc; then \
		echo "source $(PWD)/make_completion.bash" >> ~/.bashrc; \
		echo "Added completion to ~/.bashrc. Restart your shell or run 'source ~/.bashrc' to activate."; \
	else \
		echo "Completion already set up in ~/.bashrc."; \
	fi

# =============================================================================
# Help
# =============================================================================

help:
	@echo "Available targets:"
	@echo "  Native builds (host architecture):"
	@echo "    configure    - Configure the project with CMake/Ninja"
	@echo "    build        - Build the project (configures if needed)"
	@echo "    test         - Run tests"
	@echo "    clean        - Remove build directory"
	@echo ""
	@echo "  Cross builds (Docker):"
	@echo "    all                  - Alias for docker-build (default)"
	@echo "    docker-install       - Install Docker if not present"
	@echo "    docker-image         - Build Docker image if not exists"
	@echo "    docker-configure     - Configure project with Docker/Ninja"
	@echo "    docker-build         - Build project with Docker/Ninja"
	@echo "    docker-clean         - Clean Docker build artifacts"
	@echo "    docker-shell         - Start Docker shell for development"
	@echo ""
	@echo "  Deploy & Run (on target device):"
	@echo "    deploy       - Deploy to all devices (deploy-a + deploy-b)"
	@echo "    deploy-a     - Deploy lib and bin to $(IP_A)"
	@echo "    deploy-b     - Deploy lib and bin to $(IP_B)"
	@echo "    run-a        - Run test app on $(IP_A) (usage: make run-a [name] [args...], no args = run all)"
	@echo "    run-b        - Run test app on $(IP_B) (usage: make run-b [name] [args...], no args = run all)"
	@echo "    list         - List available test applications"
	@echo ""
	@echo "  Code quality & Docs:"
	@echo "    format       - Format code with clang-format"
	@echo "    cppcheck     - Run static analysis with cppcheck"
	@echo "    doc          - Generate Doxygen documentation"
	@echo ""
	@echo "  Misc:"
	@echo "    setup-completion - Set up bash completion for make (adds to ~/.bashrc)"
