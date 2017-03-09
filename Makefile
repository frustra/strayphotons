UNAME := $(shell uname)

.PHONY: auto compile linux unix windowws vs14 clean unit-tests \
	integration-tests tests astyle dependencies assets

auto: build unix compile

compile:
	cd build; make -j5

linux: unix
unix: build boost-debug-unix
	cd build; cmake -DSP_PACKAGE_RELEASE=0 -G "Unix Makefiles" ..

linux-release: unix-release
unix-release: build boost-release-unix
	cd build; cmake -DCMAKE_BUILD_TYPE=Release -DSP_PACKAGE_RELEASE=0 -G "Unix Makefiles" ..

linux-package-release: unix-package-release
unix-package-release: build boost-release-unix
	cd build; cmake -DCMAKE_BUILD_TYPE=Release -DSP_PACKAGE_RELEASE=1 -G "Unix Makefiles" ..

windows: vs14
vs14: build boost-debug-windows
	cd build; cmake -DSP_PACKAGE_RELEASE=0 -G "Visual Studio 14" ..

windows-release: vs14-release
vs14-release: build boost-release-windows
	cd build; cmake -DCMAKE_BUILD_TYPE=Release -DSP_PACKAGE_RELEASE=0 --G "Visual Studio 14" ..

windows-package-release: vs14-package-release
vs14-package-release: build boost-release-windows
	cd build; cmake -DCMAKE_BUILD_TYPE=Release -DSP_PACKAGE_RELEASE=1 -G "Visual Studio 14" ..

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

assets:
	cd assets; bash -c 'tar -cf ../bin/assets.spdata cache fonts logos scenes textures \
		`find models -name "*.gltf" -o -name "*.bin" -o -name "*.png" -o -name "*.tga"` \
		-C ../src shaders'

dependencies:
	git submodule sync
	git submodule update --init --recursive

boost-compile-libs-release:
	cd ext/boost && ./b2 link=static variant=release filesystem
	cd ext/boost && ./b2 link=static variant=release system

boost-compile-libs-debug:
	cd ext/boost && ./b2 link=static variant=debug filesystem
	cd ext/boost && ./b2 link=static variant=debug system

boost-release-windows: build ext/boost/b2.exe boost-compile-libs-release
	cp ext/boost/bin.v2/libs/filesystem/build/msvc-14.0/release/link-static/threading-multi/libboost_filesystem-*.lib build/libboost_filesystem.lib
	cp ext/boost/bin.v2/libs/system/build/msvc-14.0/release/link-static/threading-multi/libboost_system-*.lib build/libboost_system.lib
	cp ext/boost/bin.v2/libs/filesystem/build/msvc-14.0/release/link-static/threading-multi/libboost_filesystem-*.lib build/
	cp ext/boost/bin.v2/libs/system/build/msvc-14.0/release/link-static/threading-multi/libboost_system-*.lib build/

boost-debug-windows: build ext/boost/b2.exe boost-compile-libs-debug
	cp ext/boost/bin.v2/libs/filesystem/build/msvc-14.0/debug/link-static/threading-multi/libboost_filesystem-*.lib build/libboost_filesystem_DEBUG.lib
	cp ext/boost/bin.v2/libs/system/build/msvc-14.0/debug/link-static/threading-multi/libboost_system-*.lib build/libboost_system_DEBUG.lib
	cp ext/boost/bin.v2/libs/filesystem/build/msvc-14.0/debug/link-static/threading-multi/libboost_filesystem-*.lib build/
	cp ext/boost/bin.v2/libs/system/build/msvc-14.0/debug/link-static/threading-multi/libboost_system-*.lib build/

boost-release-unix: build ext/boost/b2 boost-compile-libs-release
	cp ext/boost/bin.v2/libs/filesystem/build/*/release/link-static/libboost_filesystem.a build/libboost_filesystem.a
	cp ext/boost/bin.v2/libs/system/build/*/release/link-static/libboost_system.a build/libboost_system.a

boost-debug-unix: build ext/boost/b2 boost-compile-libs-debug
	cp ext/boost/bin.v2/libs/filesystem/build/*/debug/link-static/libboost_filesystem.a build/libboost_filesystem_DEBUG.a
	cp ext/boost/bin.v2/libs/system/build/*/debug/link-static/libboost_system.a build/libboost_system_DEBUG.a

ext/boost/b2.exe:
	cd ext/boost && cmd //c bootstrap.bat

ext/boost/b2:
	cd ext/boost && ./bootstrap.sh


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

physx-windows-release:
	cd . && cmd //c physx-windows-release.bat
	mkdir -p vendor/lib/physx build
	cp ext/physx/PhysXSDK/Lib/vc14win32/*.lib vendor/lib/physx
	cp ext/physx/PhysXSDK/Bin/vc14win32/*.dll build

physx-windows-debug:
	cd . && cmd //c physx-windows-debug.bat
	mkdir -p vendor/lib/physx build
	cp ext/physx/PhysXSDK/Lib/vc14win32/*.lib vendor/lib/physx
	cp ext/physx/PhysXSDK/Bin/vc14win32/*.dll build
