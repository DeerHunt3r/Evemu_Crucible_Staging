# Base image for building EVEmu using Debian 12
FROM debian:12 AS base

# Install build dependencies
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
    passwd \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Build stage
FROM base AS app-build

# Add project files
COPY CMakeLists.txt /src/
COPY config.h.in /src/
COPY /cmake/ /src/cmake
COPY /dep/ /src/dep
COPY /src/ /src/src
COPY /utils/ /src/utils

# Included for cmake to read git rev-hash (if present)
COPY /.git/ /src/.git

# Create necessary directories
RUN mkdir -p /src/build /app /app/logs /app/server_cache /app/image_cache

ENV MYSQL_INCLUDE_DIR="/usr/include/mariadb"
ENV MYSQL_LIBRARIES="/usr/lib/x86_64-linux-gnu/libmariadbclient.so"

# Set working directory
WORKDIR /src/build

# Configure and build the project
RUN cmake -DCMAKE_INSTALL_PREFIX=/app -DCMAKE_BUILD_TYPE=Debug ..
RUN make -j$(nproc)
RUN make install

# Bake build fingerprint into the install tree (no need for git inside runtime container)
RUN mkdir -p /app/etc && \
    (cd /src && git rev-parse HEAD > /app/etc/build_git_hash.txt 2>/dev/null || echo "nogit" > /app/etc/build_git_hash.txt) && \
    date -u +"%Y-%m-%dT%H:%M:%SZ" > /app/etc/build_utc.txt

# Final runtime image
FROM base AS app

LABEL description="EVEmu Server"

# Copy built assets
COPY --from=app-build /src/utils/ /src/utils
COPY --from=app-build /app/ /app

# ? IMPORTANT: copy source tree into runtime so you can verify the exact files being built/running
COPY --from=app-build /src/src/ /src/src
COPY --from=app-build /src/CMakeLists.txt /src/CMakeLists.txt
COPY --from=app-build /src/config.h.in /src/config.h.in

# Add SQL loading tools
COPY /sql/ /src/sql

# Run SQL tool script
RUN cd /src/sql && ./get_evedbtool.sh

# Expose server ports
EXPOSE 26000
EXPOSE 26001

# Default command
CMD ["/src/utils/container-scripts/start.sh"]
