INSTALL_PREFIX = $(CURDIR)
BUILD_TYPE ?= Release

.PHONY: all build install clean

all: build install

build:
	conan install . --build=missing
	cmake --preset conan-release -DCMAKE_INSTALL_PREFIX=$(INSTALL_PREFIX)
	cmake --build --preset conan-release

install:
	cmake --install build/$(BUILD_TYPE)

clean:
	rm -rf build bin lib
