UNAME := $(shell uname)
BASE_DIR := $(shell pwd)
NPROCS := 1

ifeq ($(UNAME),Linux)
	NPROCS := $(shell grep -c ^processor /proc/cpuinfo)
else ifeq ($(UNAME),Darwin) # Assume Mac OS X
	NPROCS := $(shell system_profiler | awk '/Number Of CPUs/{print $4}{next;}')
endif

.PHONY: auto compile linux unix windowws vs14 clean unit-tests \
	integration-tests tests astyle dependencies assets

auto: unix

compile:
	cd build; make -j$(NPROCS)

linux: unix
unix: build
	cd build; CC=clang CXX=clang++ cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DSP_PACKAGE_RELEASE=0 -G "Unix Makefiles" ..; make -j$(NPROCS)

linux-release: unix-release
unix-release: build
	cd build; CC=clang CXX=clang++ cmake -DCMAKE_BUILD_TYPE=Release -DSP_PACKAGE_RELEASE=0 -G "Unix Makefiles" ..; make -j$(NPROCS)

linux-package-release: unix-package-release
unix-package-release: build assets
	cd build; CC=clang CXX=clang++ cmake -DCMAKE_BUILD_TYPE=Release -DSP_PACKAGE_RELEASE=1 -G "Unix Makefiles" ..; make -j$(NPROCS)
	rm -rf strayphotons
	mkdir -p strayphotons/bin
	cp bin/Release/sp strayphotons/bin/strayphotons
	cp extra/strayphotons.sh strayphotons
	cp bin/assets.spdata strayphotons/bin
	cp ext/fmod/lib/x86_64/libfmod.so.8.11 strayphotons/bin/libfmod.so.8
	cp ext/fmod/lib/x86_64/libfmodstudio.so.8.11 strayphotons/bin/libfmodstudio.so.8

windows: vs14
vs14: build
	cd build; cmake -DSP_PACKAGE_RELEASE=0 -G "Visual Studio 14 2015" ..

windows-release: vs14-release
vs14-release: build
	cd build; cmake -DCMAKE_BUILD_TYPE=Release -DSP_PACKAGE_RELEASE=0 -G "Visual Studio 14 2015" ..

windows-package-release: vs14-package-release
vs14-package-release: build
	cd build; cmake -DCMAKE_BUILD_TYPE=Release -DSP_PACKAGE_RELEASE=1 -G "Visual Studio 14 2015" ..

clean:
	rm -rf build bin strayphotons

build:
	mkdir -p build

unit-tests: build unix
	cd build; make unit-tests

integration-tests: build unix
	cd build; make integration-tests

cpp-check:
	cd tests; cppcheck --project=$(BASE_DIR)/build/compile_commands.json -i $(BASE_DIR)/ext/ -j$(NPROCS) 2> cppcheck-result.txt

tests: unit-tests integration-tests cpp-check

static-analysis:
	cd build; scan-build make -B -j$(NPROCS)

astyle:
	astyle --options="extra/astyle.config" "src/*.hh" "src/*.cc"

assets:
	mkdir -p bin
	cd assets; bash -c 'tar -cf ../bin/assets.spdata cache fonts logos scenes textures \
		`find models -name "*.gltf" -o -name "*.bin" -o -name "*.png" -o -name "*.tga"` \
		-C ../src shaders'

dependencies:
	git submodule sync
	git submodule update --init --recursive

watch-scenes:
	cd assets; nodemon --watch scenes -e ejs,scene --exec bash sp-scenes.sh
