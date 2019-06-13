UNAME := $(shell uname)

.PHONY: auto compile linux unix windowws vs14 clean unit-tests \
	integration-tests tests astyle dependencies assets

auto: build unix

compile:
	cd build; make -j5

linux: unix
unix: build
	cd build; cmake -DSP_PACKAGE_RELEASE=0 -G "Unix Makefiles" ..; make -j5

linux-release: unix-release
unix-release: build
	cd build; cmake -DCMAKE_BUILD_TYPE=Release -DSP_PACKAGE_RELEASE=0 -G "Unix Makefiles" ..; make -j5

linux-package-release: unix-package-release
unix-package-release: build assets
	cd build; cmake -DCMAKE_BUILD_TYPE=Release -DSP_PACKAGE_RELEASE=1 -G "Unix Makefiles" ..; make -j5
	rm -rf strayphotons
	mkdir -p strayphotons/bin
	cp bin/Release/sp strayphotons/bin/strayphotons
	cp extra/strayphotons.sh strayphotons
	cp bin/assets.spdata strayphotons/bin
	cp vendor/lib/physx/libPhysX3CharacterKinematic_x64.so strayphotons/bin
	cp vendor/lib/physx/libPhysX3Common_x64.so strayphotons/bin
	cp vendor/lib/physx/libPhysX3Cooking_x64.so strayphotons/bin
	cp vendor/lib/physx/libPhysX3_x64.so strayphotons/bin
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

tests: unit-tests integration-tests

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

watch-scenes:
	cd assets; nodemon --watch scenes -e ejs,scene --exec bash sp-scenes.sh
