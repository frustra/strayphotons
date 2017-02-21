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

- Windows: cmake, Visual Studio 14 (2015), Git Bash, GNU Make. Use Git Bash as your shell for the scripts to work.

- All: `make dependencies`

### Build

- Linux: `make physx` (if not done already), `make` or `make unix-release`.

- Windows: `make physx-windows-release` (if not done already), then `make windows-release`, then open build/sp.sln in Visual Studio.
Set the startup project to `sp`. Set the target to `RelWithDebInfo` under normal usage. Press the play button to build and run.
To create a debug build, use `make windows` and set the target to `Debug`.

### Running

- Linux/Mac: `./run.sh --map <map> --cvar "key val"`
  - On Mac, the `--basic-renderer` flag is required.

- Windows: set command line arguments in Visual Studio via Debug->sp Properties->Debugging.

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
