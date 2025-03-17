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

format:
	clang-format -i *.c src/*.c include/*.h tests/*.c

bench:
	gcc -D_GNU_SOURCE bench_arena.c src/arena.c src/lock.c \
	 -lpthread -O3 -std=c23 -Wall -Wextra -Wpedantic -march=native -flto
	./a.out 4096 100000 8

# sudo wget -O /usr/share/d3-flame-graph/d3-flamegraph-base.html https://cdn.jsdelivr.net/npm/d3-flame-graph@4/dist/templates/d3-flamegraph-base.html
perf:
	perf record ./a.out 1024 100000 4 && perf report

