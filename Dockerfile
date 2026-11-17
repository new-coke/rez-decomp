# The build environment for any host with Docker (macOS, Windows, Linux): x86-64 Debian with the
# i386 libraries the 32-bit ee-gcc needs. The repository is mounted, not copied:
#
#   docker build --platform linux/amd64 -t rez .
#   docker run --rm --platform linux/amd64 -v "$PWD":/rez rez make setup build verify
FROM debian:trixie-slim
RUN dpkg --add-architecture i386 && apt-get update -qq && apt-get install -y -qq --no-install-recommends \
    libc6:i386 libstdc++6:i386 zlib1g:i386 curl ca-certificates xz-utils bzip2 python3 git make \
    file binutils >/dev/null && rm -rf /var/lib/apt/lists/*
RUN git config --system --add safe.directory /rez
WORKDIR /rez
