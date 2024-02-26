build:
	cmake -S . -B ./build
	cmake --build build --config Debug --target all

clean:
	rm -rf build

test: build
	GTEST_COLOR=1 ctest --test-dir build --output-on-failure -j8

install:
	sudo cmake --install build --prefix=/usr/include

# Add phony target
.PHONY: build clean test install
