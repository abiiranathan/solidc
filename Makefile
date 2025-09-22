# === Configurable Variables ===

# CROSS_COMPILER: Path to the cross-compiler for Windows builds
CROSS_COMPILER ?= /usr/bin/x86_64-w64-mingw32-cc

# SYSTEM_NAME: The name of the target operating system.
# Allowed values: Linux, Windows, WebAssembly
SYSTEM_NAME ?= Linux

# RELEASE: The build type.
# Allowed values: Debug, Release
RELEASE ?= Debug

# TARGET: Build target environment.
# Allowed values:
#   native     - Build for the host system
#   cross      - Cross-compile for Windows using MinGW
#   wasm       - Compile to WebAssembly (requires emcc)
TARGET ?= native

# Toolchain paths
NATIVE_CC = gcc
WASM_CC = emcc
BUILD_DIR = build/$(TARGET)
CMAKE_C_FLAGS += -std=c23

# Phony targets
.PHONY: all configure build test install clean format bench perf

all: build

# Target-specific configurations
ifeq ($(TARGET),cross)
    CMAKE_ARGS += -DCMAKE_SYSTEM_NAME=$(SYSTEM_NAME)
    CMAKE_ARGS += -DCMAKE_C_COMPILER=$(CROSS_COMPILER)
else ifeq ($(TARGET),wasm)
    CMAKE_ARGS += -DCMAKE_SYSTEM_NAME=WASI
    CMAKE_ARGS += -DCMAKE_C_COMPILER=$(WASM_CC)
else
    CMAKE_ARGS += -DCMAKE_C_COMPILER=$(NATIVE_CC)
endif

CMAKE_ARGS += -DCMAKE_BUILD_TYPE=$(RELEASE)
CMAKE_ARGS += -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
CMAKE_ARGS += -GNinja

# Build rules
configure:
	@echo "Configuring for target: $(TARGET)"
	rm -rf $(BUILD_DIR) .cache
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake $(CMAKE_ARGS) ../..

build: configure
	@echo "Building for target: $(TARGET)"
	cd $(BUILD_DIR) && ninja

test: build
	@echo "Running tests for target: $(TARGET)"
	cd $(BUILD_DIR) && ctest --output-on-failure

install:
	@echo "Installing for target: $(TARGET)"
	cd $(BUILD_DIR) && sudo ninja install

clean:
	rm -rf build .cache a.out

format:
	find . -name '*.c' -o -name '*.h' | xargs clang-format -i

# Benchmarking (native only)
bench:
ifeq ($(TARGET),native)
	$(NATIVE_CC) -D_GNU_SOURCE benchmarks/bench_arena.c src/arena.c src/lock.c \
		-lpthread -O3 -std=c23 -Wall -Wextra -Wpedantic -Wno-unused-function -march=native -flto
	./a.out 4096 1000000 8
else
	@echo "Benchmarking only supported for native target"
endif

perf:
ifeq ($(TARGET),native)
	perf record ./a.out 1024 100000 4 && perf report
else
	@echo "Performance profiling only supported for native target"
endif

# Convenience targets
native:
	$(MAKE) TARGET=native

cross:
	$(MAKE) TARGET=cross

wasm:
	$(MAKE) TARGET=wasm

debug:
	$(MAKE) RELEASE=Debug

release:
	$(MAKE) RELEASE=Release
