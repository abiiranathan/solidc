.PHONY: configure build test install

configure:
	rm -rf build .cache
	mkdir -p build
	cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

build: 
	cd build && ninja

test: build
	cd build && ninja test

install:
	cd build && sudo ninja install

bench:
	gcc -D_GNU_SOURCE bench_arena.c src/arena.c src/lock.c src/memory_pool.c \
	 -lpthread -O3 -std=c23 -Wall -Wextra -Wpedantic -march=native
	./a.out 4096 10000 8