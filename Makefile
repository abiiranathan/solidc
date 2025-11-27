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
CMAKE_ARGS += $(CMAKE_EXTRA_FLAGS)

# === Phony Targets ===

.PHONY: all configure build test install clean format bench

all: build

configure:
	@echo "Configuring with $(CC) for $(BUILD_TYPE) build"
	@echo "Install prefix: $(INSTALL_PREFIX)"
	rm -rf $(BUILD_DIR) .cache
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake $(CMAKE_ARGS) ..

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
		-lpthread -O3 -std=c23 -Wall -Wextra -Wpedantic \
		-Wno-unused-function -march=native -o bench_arena
	./bench_arena 4096 1000000 8
	rm -f bench_arena

# === Convenience Targets ===

debug:
	$(MAKE) BUILD_TYPE=Debug

release:
	$(MAKE) BUILD_TYPE=Release

docs:
	doxygen Doxyfile