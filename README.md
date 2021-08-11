Stray Photons
=============

Build setup [![Build status](https://badge.buildkite.com/6ad6424eb4ac47ecf0738dfa96d3f011019a39d7b6066c363e.svg?branch=master)](https://buildkite.com/frustra/strayphotons)
-----------

### Install latest graphics drivers

- OpenGL >= 4.3 is required, with certain 4.4/4.5 extensions (notably ARB_direct_state_access).
Try to get the latest drivers for your hardware.

### Install build dependencies

- Linux: `cmake build-essential astyle xorg-dev libxrandr-dev mesa-common-dev libgl1-mesa-dev libglu1-mesa-dev` (or equivalent for your distro).
There might be some missing dependencies in this list, add any you find.

- Windows: cmake >= 3.13, Visual Studio 2019 Community Edition, Git Bash, GNU Make. Use Git Bash as your shell for the scripts to work.
  - On Windows, using Visual Studio Code as your editor is recommended as it is faster than using the full Visual Studio. Installing Visual Studio 2019 is still required to get the MSVC compiler.

- All: `make dependencies`

### Build

- Unix: `make` or `make unix-release`.

- Windows: then `make windows-release`, then open build/sp.sln in Visual Studio.
If using CMake <3.6, set the startup project to `sp`. Set the target to `RelWithDebInfo` under normal usage. Press the play button to build and run.
To create a debug build, use `make windows` and set the target to `Debug`.

### Running

- Unix: `./run.sh --map <map> --cvar "key val"`

- Windows: set command line arguments in Visual Studio via Debug->sp Properties->Debugging.

### Assets

When making changes to certain assets, build tools need to be run.
These tools require installing node.

- To compile a `.scene`, use `./sp-scenes.sh` in the assets folder.
- To automatically compile scenes when the files change, `npm install -g nodemon` and run `make watch-scenes`.
- To compile a model, use `./sp-convert.sh <path to obj>`.

Developing
----------

Try to remember to run astyle before committing. On Linux, `make astyle`.
