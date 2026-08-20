FROM ubuntu:22.04 AS gaelco-loader-build

WORKDIR /gaelco-loader

# The gameport binary is 32-bit (i386), so enable Ubuntu multiarch.
RUN dpkg --add-architecture i386 && \
    apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
        build-essential \
        gcc-multilib \
        g++-multilib \
        libc6-dev:i386 \
        libgl1-mesa-dev:i386 \
        libglu1-mesa-dev:i386 \
        freeglut3-dev:i386 \
        libx11-dev:i386 \
        libpthread-stubs0-dev:i386 \
        pkg-config \
        file \
        binutils \
        && \
    rm -rf /var/lib/apt/lists/*

COPY . .

RUN ls -la

# Build your loader here.
# Replace this with whatever build command your project uses.
# RUN make

# Useful sanity checks:
# RUN file ./gameport
# RUN ldd ./gameport
