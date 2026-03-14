.PHONY: all build install clean

all: build install

build:
	conan install . --build=missing
	cmake --preset conan-release -DCMAKE_INSTALL_PREFIX=/usr/local
	cmake --build --preset conan-release

install:
	cmake --install build/Release

clean:
	rm -rf build

lua:
	./scripts/build_lua.sh

ci: lua build install
