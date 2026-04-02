FROM nvcr.io/nvidia/l4t-base:r35.1.0 

ARG UID=1001
ARG GID=1001

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
# woarkaround for "Docker build hangs when adding user with a large value of user ID / UID"
# https://stackoverflow.com/questions/73351423/docker-build-hangs-when-adding-user-with-a-large-value-of-user-id-uid
RUN groupadd -g ${GID:-1001} jetson && \
useradd -m --no-log-init --uid ${UID:-1001} --gid ${GID:-1001} jetson && \
usermod -aG sudo jetson

USER jetson

# Enable ccache: prepend ccache symlinks to PATH so gcc/g++/cc/c++ go through ccache
ENV PATH="/usr/lib/ccache:${PATH}"

# Create ccache directory so Docker volume mount inherits correct ownership
RUN mkdir -p /home/jetson/.ccache

# Set working directory for build
WORKDIR /workspace
