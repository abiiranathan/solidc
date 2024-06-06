.PHONY: configure build test install

configure:
	rm -rf build .cache
	mkdir -p build
	cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

build: 
	/usr/bin/cmake --build ./build --config Debug --target all -- -j10

test: build
	cd build && ctest -j10 -C Debug -T test --output-on-failure

install: test
	cd build && sudo cmake --install . --config Debug
