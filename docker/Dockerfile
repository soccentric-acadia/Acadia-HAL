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

# Install build dependencies
RUN apt-get update && apt-get install -y build-essential cmake git sudo


RUN apt-get install -y sudo  file git

RUN groupadd -g ${GID:-1001} jetson && useradd -m --uid ${UID:-1001} --gid ${GID:-1001} jetson && usermod -aG sudo jetson

USER jetson

# Set working directory for build
WORKDIR /workspace