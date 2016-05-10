Stray Photons
=============

Build setup
-----------

### Install latest graphics drivers

- OpenGL 4.5 is required

### Install build dependencies

- Linux: `cmake build-essential astyle xorg-dev libxrandr-dev mesa-common-dev libgl1-mesa-dev libglu1-mesa-dev` (or equivalent for your distro).
There might be some missing dependencies in this list, add any you find.

- Windows: cmake, Visual Studio 14 (2015).
We'll set something up with git bash to be able to run our scripts.

- All: `make dependencies`

### Build

- Linux: `make`, and run `./bin/Debug/sp`

- Windows: `./windows_cmake.sh`, open build/sp.sln and build in Visual Studio. To launch the binary from VS, you need to set the startup project to `sp`


Developing
----------

Try to remember to run astyle before committing. On Linux, `make astyle`.
