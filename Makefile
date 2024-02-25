build:
	cmake --build build -j8

test: build
	GTEST_COLOR=1 ctest --test-dir build --output-on-failure -j8

install:
	sudo cmake --install build --prefix=/usr/include