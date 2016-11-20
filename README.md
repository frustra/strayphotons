Stray Photons
=============

Build setup
-----------

### Install latest graphics drivers

- OpenGL >= 4.3 is required, with certain 4.4/4.5 extensions (notably ARB_direct_state_access).
Try to get the latest drivers for your hardware.

### Install build dependencies

- Linux: `cmake build-essential astyle xorg-dev libxrandr-dev mesa-common-dev libgl1-mesa-dev libglu1-mesa-dev` (or equivalent for your distro).
There might be some missing dependencies in this list, add any you find.

- Windows: cmake, Visual Studio 14 (2015).
We'll set something up with git bash to be able to run our scripts.

- All: `make dependencies`

### Build

- Linux: `make`, and run `./bin/Debug/sp`

- Windows: `./windows_cmake.sh`, open build/sp.sln and build in Visual Studio. To launch the binary from VS, you need to set the startup project to `sp`

### Audio

If you don't hear any sound it might be because the correct audio driver or
output type is not the default. You can change this with the following
environment variables:

**AUDIO_DRIVER:** Set to a number (default 0). View logs near
"[log] Using audio driver 0 of 3" for a list of choices.

**AUDIO_OUTPUT_TYPE:** Set to either "pulseaudio" or "alsa" on Linux.

For example: ```AUDIO_DRIVER=2 AUDIO_OUTPUT_TYPE="alsa" ./run.sh```

Developing
----------

Try to remember to run astyle before committing. On Linux, `make astyle`.
