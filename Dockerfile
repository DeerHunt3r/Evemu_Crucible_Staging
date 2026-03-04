# EVEmu Crucible - Dev Runtime (single-container workflow)
# Goal: edit source on host, compile + run in the same container without image rebuilds.

FROM debian:12

# Build + runtime deps
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    curl \
    wget \
    zlib1g-dev \
    libmariadb-dev \
    libboost-all-dev \
    libtinyxml-dev \
    ca-certificates \
    g++ \
    gdb \
    libutfcpp-dev \
    mariadb-client \
    pkg-config \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Keep legacy expectations (your compose mounts these)
RUN mkdir -p /app/etc /app/logs /app/server_cache /app/image_cache

# Create a persistent source+build workspace
RUN mkdir -p /src /src/build

# Dev start script:
# - configures cmake once (or when cache is missing)
# - builds every start (so edits are always compiled)
# - runs the freshly built binary
RUN cat > /usr/local/bin/evemu-dev-start.sh << 'EOF' && chmod +x /usr/local/bin/evemu-dev-start.sh
#!/bin/sh
set -eu

SRC="/src"
BUILD="/src/build"

echo "=================================================================="
echo "EVEmu dev start"
echo "  SRC  : ${SRC}"
echo "  BUILD: ${BUILD}"
echo "=================================================================="

if [ ! -f "${SRC}/CMakeLists.txt" ]; then
  echo "ERROR: ${SRC}/CMakeLists.txt not found."
  echo "This container expects your repo bind-mounted to /src."
  exit 1
fi

mkdir -p "${BUILD}"

# Show exactly what source revision we are compiling (if .git is present)
if [ -d "${SRC}/.git" ]; then
  echo "Git hash (host repo, inside container): $(cd "${SRC}" && git rev-parse HEAD)"
else
  echo "Git hash: (no .git present inside /src)"
fi

cd "${BUILD}"

# Configure once (or if build dir was wiped)
if [ ! -f "CMakeCache.txt" ]; then
  echo "[cmake] configuring..."
  cmake -DCMAKE_BUILD_TYPE=Debug ..
fi

echo "[make] building..."
make -j"$(nproc)"

BIN="${BUILD}/src/eve-server/eve-server"
if [ ! -x "${BIN}" ]; then
  echo "ERROR: server binary not found at ${BIN}"
  echo "If your build outputs elsewhere, we can adjust this path."
  exit 1
fi

echo "Built binary: ${BIN}"
echo "Binary timestamp: $(ls -l "${BIN}")"
echo "=================================================================="

# Optional: run under gdb if enabled
if [ "${RUN_WITH_GDB:-FALSE}" = "TRUE" ] || [ "${RUN_WITH_GDB:-FALSE}" = "true" ]; then
  exec gdb --args "${BIN}"
fi

exec "${BIN}"
EOF

EXPOSE 26000 26001

WORKDIR /src/build
CMD ["/usr/local/bin/evemu-dev-start.sh"]
