.PHONY: all test clean install

all:
	cmake -B build -DCMAKE_BUILD_TYPE=Release
	cmake --build build

test:
	ctest --test-dir build --output-on-failure

install:
	cmake --install build

clean:
	rm -rf build
