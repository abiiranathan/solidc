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
	 -lpthread -O2 -std=c23 -Wall -Wextra -Wpedantic -march=native -DARENA_NOLOCK=1
	./a.out 4096 100000 8

# sudo wget -O /usr/share/d3-flame-graph/d3-flamegraph-base.html https://cdn.jsdelivr.net/npm/d3-flame-graph@4/dist/templates/d3-flamegraph-base.html
perf:
	perf record ./a.out 4096 100000 8 && perf report

