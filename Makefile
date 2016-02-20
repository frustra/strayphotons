auto: build unix compile

compile:
	cd build; make

linux: unix
unix: build
	cd build; cmake -G "Unix Makefiles" ..

windows: vs10
vs10: build
	cd build; cmake -G "Visual Studio 10" ..

clean:
	rm -rf build bin

build:
	mkdir -p build

astyle:
	astyle --options="extra/astyle.config" "include/*.hh" "src/*.cc"

dependencies:
	git submodule update --init --recursive

