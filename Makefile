# Master Makefile for Acadia HAL
# Docker-based builds: the default (and only) build path is Docker.

.DEFAULT_GOAL := help

.PHONY: all build clean test format cppcheck lint doc help
.PHONY: package package-deb package-rpm package-ipk
.PHONY: docker-build docker-configure docker-clean docker-shell docker-install docker-image docker-test
.PHONY: docker-package docker-package-deb docker-package-rpm docker-package-ipk
.PHONY: deploy deploy-a deploy-b run-a run-b list setup-completion

# Build directories (separate to avoid CMake cache conflicts)
CROSS_BUILD_DIR := build/cross

# Parallel jobs (override with make JOBS=N)
JOBS := $(shell nproc)
DOCKER_IMAGE := acadia
DOCKER_CROSS_BUILD_DIR := /workspace/build/cross

# Host architecture as shown by file(1) (e.g. x86-64, aarch64)
HOST_ARCH := $(shell uname -m | sed 's/x86_64/x86-64/')

# IP addresses for Raspberry Pi 5 target devices
# Single source of truth: loop/tools/targets.yaml. Do not duplicate a second
# copy here; provide the address at call time (make deploy-a IP_A=... or
# ACADIA_TARGET_ADDRESS=...). Empty default so a stale/wrong target can never be
# silently deployed to.
IP_A ?= $(ACADIA_TARGET_ADDRESS)
IP_B ?=

# SSH credentials are NEVER committed. Injected from the environment / secret
# manager at run time. Key-based auth is preferred (ACADIA_TARGET_SSH_KEY); a
# password may be supplied via ACADIA_TARGET_PASS. Deploy targets fail if
# neither is configured.
USER := $(or $(ACADIA_TARGET_USER),$(ACADIA_SSH_USER),$(HAL_SSH_USER))
PASS := $(or $(ACADIA_TARGET_PASS),$(HAL_SSH_PASS))
SSHPASS := $(if $(PASS),sshpass -p $(PASS))
SSH := $(SSHPASS) ssh -o StrictHostKeyChecking=no
SCP := $(SSHPASS) scp -o StrictHostKeyChecking=no

# Default target
all: docker-build ## Default target: Docker cross-build (fleet standard)

# Deploy to remote target: $(1)=IP address
# Fails fast if no address or credentials were provided.
_require_deploy_env = $(if $(strip $(1)),,$(error deploy requires an IP (set IP_A/IP_B or ACADIA_TARGET_ADDRESS)))
# HAL-062: versioned deploy path (no more /tmp staging). Each deploy lands in
# /opt/acadia/hal/<version> with a SHA-256 manifest, so bench targets keep
# multiple versions side by side and nothing silently overwrites a running one.
HAL_VERSION := $(shell grep -m1 'ACUFullTest VERSION' CMakeLists.txt | sed 's/.*VERSION \([0-9.]*\).*/\1/')
ACADIA_DEPLOY_ROOT ?= /opt/acadia/hal
DEPLOY_DIR := $(ACADIA_DEPLOY_ROOT)/$(HAL_VERSION)

define deploy_remote
	$(call _require_deploy_env,$(1))
	$(if $(and $(PASS),$(SSHPASS)),,$(if $(ACADIA_TARGET_SSH_KEY),,$(error deploy: no SSH credentials configured; set ACADIA_TARGET_SSH_KEY or ACADIA_TARGET_USER/ACADIA_TARGET_PASS)))
	@echo "Deploying HAL $(HAL_VERSION) to $(1):$(DEPLOY_DIR) ..."
	$(SSH) $(USER)@$(1) "rm -rf $(DEPLOY_DIR) && mkdir -p $(DEPLOY_DIR)"
	$(SCP) -r $(CROSS_BUILD_DIR)/lib $(USER)@$(1):$(DEPLOY_DIR)/
	$(SCP) -r $(CROSS_BUILD_DIR)/bin $(USER)@$(1):$(DEPLOY_DIR)/
	$(SSH) $(USER)@$(1) "cd $(DEPLOY_DIR) && find . -type f -not -name MANIFEST -exec sha256sum {} \; | sed 's|  \./|  |' > MANIFEST && echo 'version: $(HAL_VERSION)' >> MANIFEST && date -u '+deployed: %Y-%m-%dT%H:%M:%SZ' >> MANIFEST"
endef

# Run apps on remote target: $(1)=IP address
define run_remote
	$(call _require_deploy_env,$(1))
	@APP="$(word 2, $(MAKECMDGOALS))"; \
	ARGS="$(wordlist 3, $(words $(MAKECMDGOALS)), $(MAKECMDGOALS))"; \
	if [ -z "$$APP" ]; then \
		echo "Running all test applications on $(1)..."; \
		for name in $$($(SSH) $(USER)@$(1) "ls /tmp/acu-test/bin/"); do \
			echo "--- Running $$name ---"; \
			$(SSH) $(USER)@$(1) "cd /tmp/acu-test && ./bin/$$name" || true; \
		done; \
	else \
		echo "Running $$APP on $(1) with args: $$ARGS"; \
		$(SSH) $(USER)@$(1) "cd /tmp/acu-test && ./bin/$$APP $$ARGS"; \
	fi
endef

# =============================================================================
# Build & test (Docker — the default build path)
# =============================================================================

build: docker-build ## Build the project in Docker (alias for docker-build)
test: docker-test ## Run tests in Docker (ARM64 via QEMU)

# =============================================================================
# Common for native and Cross
# =============================================================================

clean: ## Remove build directory
	@echo "Cleaning build directory..."
	rm -rf build

list: ## List available test applications
	@echo "Cross ($(CROSS_BUILD_DIR)):"
	@ls -1 $(CROSS_BUILD_DIR)/bin/ 2>/dev/null | sed 's/^/    /' || echo "    (none - run 'make docker-build' first)"

# =============================================================================
# Cross builds (Docker)
# =============================================================================

# -t: allocate a pseudo-TTY so Ctrl-C (SIGINT) is forwarded to the container
# --init: use tini as PID 1 to properly propagate signals to child processes (e.g. ninja)
# ccache volume: persist compilation cache across docker runs
DOCKER_RUN := docker run --rm -t --init --platform linux/arm64 -v $(PWD):/workspace -v ccache-acu:/home/rpi/.ccache $(DOCKER_IMAGE) bash -c

docker-install: ## Install Docker if not present
	@if ! command -v docker > /dev/null 2>&1; then \
		echo "Docker not found. Installing Docker..."; \
		sudo apt update && sudo apt install -y docker.io; \
	else \
		echo "Docker is already installed."; \
	fi

docker-image: docker-install ## Build Docker image if not exists
	@if ! docker image inspect $(DOCKER_IMAGE) >/dev/null 2>&1; then \
		echo "Building Docker image $(DOCKER_IMAGE)..."; \
		make -C docker build; \
	else \
		echo "Docker image $(DOCKER_IMAGE) already exists."; \
	fi

docker-configure: docker-image ## Configure project with Docker/Ninja
	@echo "Configuring project with Docker/Ninja..."
	$(DOCKER_RUN) "mkdir -p $(DOCKER_CROSS_BUILD_DIR) && cd $(DOCKER_CROSS_BUILD_DIR) && cmake -GNinja /workspace"

docker-build: docker-image ## Build project with Docker/Ninja
	@echo "Building project with Docker/Ninja..."
	$(DOCKER_RUN) "if [ ! -f $(DOCKER_CROSS_BUILD_DIR)/build.ninja ]; then mkdir -p $(DOCKER_CROSS_BUILD_DIR) && cd $(DOCKER_CROSS_BUILD_DIR) && cmake -GNinja /workspace; fi && cd $(DOCKER_CROSS_BUILD_DIR) && ninja -j$(JOBS)"

docker-clean: docker-image ## Clean Docker build artifacts
	@echo "Cleaning Docker build directory..."
	$(DOCKER_RUN) "rm -rf $(DOCKER_CROSS_BUILD_DIR)"

docker-shell: docker-image ## Start Docker shell for development
	@echo "Starting Docker shell for development..."
	docker run -it --rm --platform linux/arm64 -v $(PWD):/workspace -v ccache-acu:/home/rpi/.ccache $(DOCKER_IMAGE) bash

docker-test: docker-build ## Run CTest inside Docker (ARM64 via QEMU)
	@echo "Running tests inside Docker (ARM64 via QEMU)..."
	$(DOCKER_RUN) "cd $(DOCKER_CROSS_BUILD_DIR) && ctest --output-on-failure -j$(JOBS)"

docker-package-deb: docker-build ## Generate DEB package (cross/ARM64)
	@echo "Generating DEB package (cross/ARM64)..."
	$(DOCKER_RUN) "cd $(DOCKER_CROSS_BUILD_DIR) && cpack -G DEB"

docker-package-rpm: docker-build ## Generate RPM package (cross/ARM64)
	@echo "Generating RPM package (cross/ARM64, requires rpmbuild in image)..."
	$(DOCKER_RUN) "cd $(DOCKER_CROSS_BUILD_DIR) && cpack -G RPM"

docker-package-ipk: docker-build ## Generate IPK package (cross/ARM64)
	@echo "Generating IPK package (cross/ARM64)..."
	$(DOCKER_RUN) "cd $(DOCKER_CROSS_BUILD_DIR) && cmake --build . --target package_ipk"

docker-package: docker-package-deb docker-package-ipk ## Generate DEB + IPK (cross/ARM64)
	@echo "Cross packages generated in build/cross/"

# =============================================================================
# Deploy & Run (on target device)
# =============================================================================

deploy: deploy-a deploy-b ## Deploy to all devices (deploy-a + deploy-b)

deploy-a: docker-build ## Deploy lib and bin to $(IP_A)
	$(call deploy_remote,$(IP_A))

deploy-b: docker-build ## Deploy lib and bin to $(IP_B)
	$(call deploy_remote,$(IP_B))

run-a: deploy-a ## Run test app on $(IP_A) (usage: make run-a [name] [args...])
	$(call run_remote,$(IP_A))

run-b: deploy-b ## Run test app on $(IP_B) (usage: make run-b [name] [args...])
	$(call run_remote,$(IP_B))

# =============================================================================
# Code quality & Docs (run inside the Docker build container)
# =============================================================================

format: docker-image ## Format code with clang-format (in Docker)
	@echo "Running clang-format in Docker..."
	$(DOCKER_RUN) "cd $(DOCKER_CROSS_BUILD_DIR) && ninja format"

cppcheck: docker-image ## Run static analysis with cppcheck (in Docker)
	@echo "Running cppcheck in Docker..."
	$(DOCKER_RUN) "cd $(DOCKER_CROSS_BUILD_DIR) && ninja cppcheck"

doc: docker-image ## Generate Doxygen documentation (in Docker)
	@echo "Generating documentation in Docker..."
	$(DOCKER_RUN) "cd $(DOCKER_CROSS_BUILD_DIR) && ninja doc_doxygen"

# Packaging: the docker-package* targets above cover all packaging (Docker).

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

# ── Fleet-standard aliases ────────────────────────────────────────────────────
# Same verbs mean the same thing in every repo; these forward to this
# repo's own targets rather than duplicating their recipes.
.PHONY: fmt
fmt: format ## Alias for format

# ── Help ──────────────────────────────────────────────────────────────────────
# Targets are self-documenting: text after `## ` on a target line is picked up
# by `make help` automatically, so the listing can never drift from the rules.
.PHONY: help
version: ## Print the project version (CONV-004)
	@echo "1.0.0"

help: ## Show this help
	@printf '\n  \033[1m%s\033[0m\n' 'Acadia-HAL'
	@printf '  Usage: make <target> [VAR=value ...]\n'
	@printf '\n'
	@awk 'BEGIN {FS = ":.*?## "} /^[a-zA-Z0-9_.\/-]+:.*?## / { printf "    \033[36m%-24s\033[0m %s\n", $$1, $$2 }' $(MAKEFILE_LIST)
	@printf '\n'
