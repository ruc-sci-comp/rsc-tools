.PHONY: all build install clean conan

all: build install

CONAN_STAMP = build/conan.stamp

$(CONAN_STAMP): conanfile.py
	conan install . --build=missing
	cmake --preset conan-release -DCMAKE_INSTALL_PREFIX=/usr/local
	touch $(CONAN_STAMP)

conan: $(CONAN_STAMP)

build: conan
	cmake --build --preset conan-release

install:
	cmake --install build/Release

clean:
	rm -rf build

lua:
	./scripts/install_lua.sh

ci: lua build install
