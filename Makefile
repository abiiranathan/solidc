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
	find . -name '*.c' -o -name '*.h' | xargs clang-format -i

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