# === Configurable Variables (override via environment or command line) ===

# CC: C compiler to use
CC ?= gcc

# INSTALL_PREFIX: Installation directory
INSTALL_PREFIX ?= /usr/local

# BUILD_TYPE: Debug or Release
BUILD_TYPE ?= Release

# BUILD_DIR: Where to place build artifacts
BUILD_DIR ?= build

# Additional CMake flags (optional)
CMAKE_EXTRA_FLAGS ?=

# === Internal Configuration ===

CMAKE_ARGS = -GNinja
CMAKE_ARGS += -DCMAKE_C_COMPILER=$(CC)
CMAKE_ARGS += -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
CMAKE_ARGS += -DCMAKE_INSTALL_PREFIX=$(INSTALL_PREFIX)
CMAKE_ARGS += -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
CMAKE_ARGS += -DBUILD_BENCHMARKS=ON
CMAKE_ARGS += $(CMAKE_EXTRA_FLAGS)

# === Phony Targets ===

.PHONY: all configure build test install clean format bench musl-static

all: build

configure:
	@echo "Configuring with $(CC) for $(BUILD_TYPE) build"
	@echo "Install prefix: $(INSTALL_PREFIX)"
	rm -rf $(BUILD_DIR) .cache
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake $(CMAKE_ARGS) $(CURDIR)

build: configure
	@echo "Building..."
	cd $(BUILD_DIR) && ninja

test: build
	@echo "Running tests..."
	cd $(BUILD_DIR) && ctest --output-on-failure

install:
	@echo "Installing to $(INSTALL_PREFIX)"
	# This installs the artifacts without triggering a build check
	sudo cmake --install $(BUILD_DIR) --prefix $(INSTALL_PREFIX)

clean:
	rm -rf $(BUILD_DIR) .cache a.out

format:
	find src/ include/ -name '*.c' -o -name '*.h' | xargs clang-format -i

bench:
	$(CC) -D_GNU_SOURCE benchmarks/bench_arena.c src/arena.c src/lock.c \
		-lpthread -O3 -std=c11 -Wall -Wextra -Wpedantic \
		-Wno-unused-function -march=native -o bench_arena
	./bench_arena
	rm -f bench_arena

# === Convenience Targets ===

debug:
	$(MAKE) BUILD_TYPE=Debug

release:
	$(MAKE) BUILD_TYPE=Release

# === Cross-Platform Checks ===
# One command to surface every -Werror failure that CI would hit on
# Linux (x64 + aarch64 cross), Windows (MinGW cross), and macOS/BSD.
# Each cross-check is skipped gracefully when its toolchain/SDK is absent,
# so `make check-platforms` is useful on a plain Linux dev box and still
# exhaustive on the CI matrix (where every runner IS native for its OS).
.PHONY: check-platforms check-linux check-linux-aarch64 check-windows check-macos check-bsd

check-platforms: check-linux check-linux-aarch64 check-windows check-macos check-bsd
	@echo ""
	@echo "All platform checks complete."

check-linux:
	@echo "=== Checking Linux x86_64 (native, Release -Werror) ==="
	@rm -rf build/check-linux
	@cmake -GNinja -S . -B build/check-linux -DCMAKE_BUILD_TYPE=Release > /dev/null
	@cmake --build build/check-linux
	@echo "Linux x86_64: OK"

check-linux-aarch64:
	@if ! command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then \
		echo "=== Checking Linux aarch64 (cross) ==="; \
		echo "SKIP: aarch64-linux-gnu-gcc not found (sudo pacman -S aarch64-linux-gnu-gcc)"; \
	else \
		echo "=== Checking Linux aarch64 (cross, -Werror) ==="; \
		rm -rf build/check-aarch64; \
		cmake -GNinja -S . -B build/check-aarch64 -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ -DCMAKE_FIND_ROOT_PATH=/usr/aarch64-linux-gnu -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DBUILD_BENCHMARKS=OFF -DBUILD_EXAMPLES=OFF > /dev/null && \
		cmake --build build/check-aarch64 --target solidc && \
		echo "Linux aarch64: OK"; \
	fi

check-windows:
	@if ! command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then \
		echo "=== Checking Windows x64 (cross) ==="; \
		echo "SKIP: x86_64-w64-mingw32-gcc not found (sudo pacman -S mingw-w64-gcc)"; \
	else \
		echo "=== Checking Windows x64 (cross, MinGW) ==="; \
		rm -rf build/check-windows; \
		cmake -GNinja -S . -B build/check-windows -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ -DCMAKE_FIND_ROOT_PATH=/usr/x86_64-w64-mingw32 -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DBUILD_BENCHMARKS=OFF -DBUILD_EXAMPLES=OFF > /dev/null && \
		cmake --build build/check-windows --target solidc && \
		echo "Windows x64: OK"; \
	fi

check-macos:
	@if [ "$$(uname)" != "Darwin" ]; then \
		echo "=== Checking macOS (native) ==="; \
		echo "SKIP: macOS checks require a Darwin host (no Apple SDK on Linux)"; \
		echo "      Verified separately: src/stdstreams.c and src/lock.c macOS paths syntax-check clean."; \
	else \
		echo "=== Checking macOS (native, Release -Werror) ==="; \
		rm -rf build/check-macos; \
		cmake -GNinja -S . -B build/check-macos -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15 > /dev/null && \
		cmake --build build/check-macos && \
		echo "macOS: OK"; \
	fi

check-bsd:
	@if [ "$$(uname)" != "FreeBSD" ] && [ "$$(uname)" != "OpenBSD" ]; then \
		echo "=== Checking BSD (native) ==="; \
		echo "SKIP: BSD checks require a BSD host (no sys/_types.h SDK on Linux)"; \
	else \
		echo "=== Checking BSD (native, Release -Werror) ==="; \
		rm -rf build/check-bsd; \
		cmake -GNinja -S . -B build/check-bsd -DCMAKE_BUILD_TYPE=Release > /dev/null && \
		cmake --build build/check-bsd && \
		echo "BSD: OK"; \
	fi

docs:
	doxygen Doxyfile

# === Musl Static Target ===
# Recursively calls 'make build' while overriding the compiler to musl-gcc
# and isolating the object files inside 'build/musl' to avoid glibc collisions.
# === Musl Static Target ===
# Recursively calls 'make build' while overriding the compiler to musl-gcc
# and isolating the object files inside 'build/musl' to avoid glibc collisions.
musl-static:
	@echo "Configuring and building static library with musl-gcc..."
	$(MAKE) CC=musl-gcc \
		BUILD_DIR=build/musl \
		BUILD_TYPE=Release \
		CMAKE_EXTRA_FLAGS="-DCMAKE_FIND_ROOT_PATH=/usr/lib/musl -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY" \
		build

# === Fil-C Target ===
# 1. Clone BlocksRuntime
# git clone https://github.com/mackyle/blocksruntime.git
# cd blocksruntime

# # 2. Build and install to /opt/fil using filcc
# ./buildlib CC=/opt/fil/bin/filcc
# sudo prefix=/opt/fil ./installib
# Recursively calls 'make build' and 'make install' overriding compiler to filcc,
# setting the install prefix to /opt/fil, and isolating artifacts inside 'build/filc'.
fil-c:
	@echo "Configuring, building, and installing with Fil-C..."
	$(MAKE) CC=/opt/fil/bin/filcc \
		INSTALL_PREFIX=/opt/fil \
		BUILD_DIR=build/filc \
		BUILD_TYPE=Release \
		CMAKE_EXTRA_FLAGS="-DCMAKE_PREFIX_PATH=/opt/fil -DCMAKE_C_FLAGS='-I/usr/include' -DCMAKE_EXE_LINKER_FLAGS='-lBlocksRuntime' -DCMAKE_SHARED_LINKER_FLAGS='-lBlocksRuntime'" \
		build install