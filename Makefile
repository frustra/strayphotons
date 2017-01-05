UNAME := $(shell uname)

.PHONY: auto compile linux unix windowws vs14 clean unit-tests \
	integration-tests tests astyle dependencies

auto: build unix compile

compile:
	cd build; make -j5

linux: unix
unix: build
	cd build; cmake -G "Unix Makefiles" ..

windows: vs14
vs14: build
	cd build; cmake -G "Visual Studio 14" ..

clean:
	rm -rf build bin

build:
	mkdir -p build

unit-tests: build unix
	cd build; make unit-tests

integration-tests: build unix
	cd build; make integration-tests

tests: unit-tests integration-tests

astyle:
	astyle --options="extra/astyle.config" "src/*.hh" "src/*.cc"

dependencies:
	git submodule sync
	git submodule update --init --recursive

ifeq ($(UNAME), Darwin)
physx:
	cd ext/physx/PhysXSDK/Source/compiler/xcode_osx64 && xcodebuild -project PhysX.xcodeproj -alltargets -configuration debug
	rm -rf vendor/lib/physx
	mkdir -p vendor/lib/physx
	cp ext/physx/PhysXSDK/Lib/osx64/lib* vendor/lib/physx
else
physx:
	cd ext/physx/PhysXSDK/Source/compiler/linux64 && make -j 16
	rm -rf vendor/lib/physx
	mkdir -p vendor/lib/physx
	cp ext/physx/PhysXSDK/Lib/linux64/lib* vendor/lib/physx
	cp ext/physx/PhysXSDK/Bin/linux64/lib* vendor/lib/physx
endif
