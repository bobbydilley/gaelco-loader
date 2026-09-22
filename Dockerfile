# Reproducible build of GaelcoLoader.AppImage.
#
#   # produce ./dist/GaelcoLoader.AppImage on the host:
#   docker buildx build --target artifact --output type=local,dest=dist .
#
#   # or build the full image and copy the artifact out of it:
#   docker build -t gaelco-loader .
#   id=$(docker create gaelco-loader); docker cp "$id:/out/GaelcoLoader.AppImage" .; docker rm "$id"

FROM ubuntu:22.04 AS build

ENV DEBIAN_FRONTEND=noninteractive

# gaelco + gaelco-preload.so are 32-bit (i386), so enable Ubuntu multiarch and
# pull the i386 toolchain and the -dev packages the loader links against.
RUN dpkg --add-architecture i386 && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
        ca-certificates \
        cmake \
        build-essential \
        gcc-multilib \
        pkg-config \
        file \
        wget \
        git \
        patchelf \
        squashfs-tools \
        desktop-file-utils \
        libsdl2-dev:i386 \
        libx11-dev:i386 \
        libxxf86vm-dev:i386 \
        libgl1-mesa-dev:i386 \
        libgl-dev:i386 \
        libglx-dev:i386 \
        libglu1-mesa-dev:i386 \
    && rm -rf /var/lib/apt/lists/*

# linuxdeploy and the appimagetool it calls cannot use FUSE inside a container.
ENV APPIMAGE_EXTRACT_AND_RUN=1

WORKDIR /src
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --parallel "$(nproc)"

RUN scripts/build-app-image && \
    mkdir -p /out && \
    cp GaelcoLoader.AppImage /out/GaelcoLoader.AppImage && \
    file /out/GaelcoLoader.AppImage

# Thin final stage: `--output type=local` then yields just the AppImage.
FROM scratch AS artifact
COPY --from=build /out/GaelcoLoader.AppImage /GaelcoLoader.AppImage
