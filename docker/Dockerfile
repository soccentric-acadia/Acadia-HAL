# Raspberry Pi 5 (BCM2712 / RP1) build container
# Uses Ubuntu 22.04 LTS for ARM64 — runs natively on Pi5 or via QEMU on x86_64
FROM ubuntu:22.04

ARG UID=1000
ARG GID=1000

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    protobuf-compiler \
    libprotobuf-dev \
    libprotoc-dev \
    libgrpc++-dev \
    libsqlite3-dev \
    binutils \
    pkg-config \
    ninja-build \
    vim \
    htop \
    libboost-system-dev \
    libboost-date-time-dev \
    && rm -rf /var/lib/apt/lists/*

# Install additional dependencies
RUN apt-get update && apt-get install -y build-essential cmake git sudo ccache file \
    && rm -rf /var/lib/apt/lists/*

# --no-log-init
# Workaround for "Docker build hangs when adding user with a large UID"
# https://stackoverflow.com/questions/73351423
RUN groupadd -g ${GID:-1000} rpi && \
    useradd -m --no-log-init --uid ${UID:-1000} --gid ${GID:-1000} rpi && \
    usermod -aG sudo rpi

USER rpi

# Enable ccache: prepend ccache symlinks to PATH so gcc/g++/cc/c++ go through ccache
ENV PATH="/usr/lib/ccache:${PATH}"

# Create ccache directory so Docker volume mount inherits correct ownership
RUN mkdir -p /home/rpi/.ccache

# Set working directory for build
WORKDIR /workspace
