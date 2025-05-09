FROM ubuntu:24.04

RUN dpkg --add-architecture i386 && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
    g++-multilib:i386 cmake:i386 nasm:i386 pkg-config:i386 libsdl2-dev:i386 libsdl2-ttf-dev:i386 libsdl2-image-dev:i386 libsdl2-mixer-dev:i386 libsodium-dev:i386 make:i386

WORKDIR /build
VOLUME /src
