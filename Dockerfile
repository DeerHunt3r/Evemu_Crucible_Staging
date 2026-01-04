# Base image for building EVEmu using Debian 12
FROM debian:12 AS base

# Install build dependencies (fix apt signature issues by forcing HTTPS + refreshing keyrings)

RUN set -eux; \
    export DEBIAN_FRONTEND=noninteractive; \
    \
    # (Optional) show clock for debugging in build logs
    date -u; \
    \
    # Force HTTP first (bootstrap), but allow insecure just for the bootstrap step
    if [ -f /etc/apt/sources.list ]; then \
        sed -i 's|https://deb.debian.org|http://deb.debian.org|g; s|https://security.debian.org|http://security.debian.org|g' /etc/apt/sources.list; \
    fi; \
    if [ -f /etc/apt/sources.list.d/debian.sources ]; then \
        sed -i 's|https://deb.debian.org|http://deb.debian.org|g; s|https://security.debian.org|http://security.debian.org|g' /etc/apt/sources.list.d/debian.sources; \
        sed -i 's|URIs: https://|URIs: http://|g' /etc/apt/sources.list.d/debian.sources; \
    fi; \
    \
    apt-get -o Acquire::AllowInsecureRepositories=true \
            -o Acquire::AllowDowngradeToInsecureRepositories=true \
            -o APT::Get::AllowUnauthenticated=true \
            update; \
    apt-get -y --no-install-recommends \
            -o APT::Get::AllowUnauthenticated=true \
            install ca-certificates debian-archive-keyring gnupg; \
    rm -rf /var/lib/apt/lists/*; \
    \
    # Now switch to HTTPS and go back to normal strict verification
    if [ -f /etc/apt/sources.list ]; then \
        sed -i 's|http://deb.debian.org|https://deb.debian.org|g; s|http://security.debian.org|https://security.debian.org|g' /etc/apt/sources.list; \
    fi; \
    if [ -f /etc/apt/sources.list.d/debian.sources ]; then \
        sed -i 's|http://deb.debian.org|https://deb.debian.org|g; s|http://security.debian.org|https://security.debian.org|g' /etc/apt/sources.list.d/debian.sources; \
        sed -i 's|URIs: http://|URIs: https://|g' /etc/apt/sources.list.d/debian.sources; \
    fi; \
    \
    apt-get update; \
    apt-get install -y --no-install-recommends \
        build-essential cmake git curl wget zlib1g-dev libmariadb-dev libboost-all-dev \
        libtinyxml-dev g++ gdb libutfcpp-dev mariadb-client passwd; \
    apt-get clean; \
    rm -rf /var/lib/apt/lists/*


# Build stage
FROM base AS app-build

# Add project files
COPY CMakeLists.txt /src/
COPY config.h.in /src/
COPY /cmake/ /src/cmake
COPY /dep/ /src/dep
COPY /src/ /src/src
COPY /utils/ /src/utils
# Included for cmake to read git rev-hash
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

# Final runtime image
FROM base AS app

LABEL description="EVEmu Server"

# Copy built assets
COPY --from=app-build /src/utils/ /src/utils
COPY --from=app-build /app/ /app

# Add SQL loading tools
COPY /sql/ /src/sql

# Run SQL tool script
RUN cd /src/sql && ./get_evedbtool.sh

# Expose server ports
EXPOSE 26000
EXPOSE 26001

# Default command
CMD ["/src/utils/container-scripts/start.sh"]

