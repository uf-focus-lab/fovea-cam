# This is a heper Makefile for CMake Project
DEPS := $(wildcard src/*.cpp)
DEPS += $(wildcard lib/inc/*.h)
DEPS += $(wildcard lib/src/*.cpp)

release: CMakeLists.txt src/*.cpp $(DEPS)
	@ mkdir -p build
	@ cd build \
		&& cmake -DCMAKE_BUILD_TYPE=Release .. \
		&& cmake --build .

debug: CMakeLists.txt src/*.cpp $(DEPS)
	@ mkdir -p debug
	@ cd debug \
		&& cmake -DCMAKE_BUILD_TYPE=Debug .. \
		&& cmake --build .

clean:
	@ rm -rf build debug

.PHONY: release debug run clean
