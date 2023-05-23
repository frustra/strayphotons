# Stray Photons [![Build status](https://badge.buildkite.com/6ad6424eb4ac47ecf0738dfa96d3f011019a39d7b6066c363e.svg?branch=master)](https://buildkite.com/frustra/strayphotons)

Stray Photons is a high performance sandbox game engine built on top of the [Tecs Entity Component System](https://github.com/xthexder/Tecs). Designed with a focus on high quality VR interactions, Stray Photons features a voxel-based GI lighting system, seamless asynchronous scene loading, full-hand physics interactions, powerful game logic signalling and scripting capabilities, and much more.

## Dependencies

- Ensure graphics drivers are up to date.
- Vulkan >= 1.2 is required. Get the SDK from https://www.vulkan.org/tools.
- CMake and a C++20 compiler (Windows: MSVC, Linux: Clang or GCC).
- Git submodules must be checked out using `git submodule update --init --recursive`
- Python is used for build scripts and CI
- Node.js is used for model GLTF conversion
- Rust is optional for building the test linked library
- VSCode is recommended, most of the workspace config is included

## License

Copyright (c) 2016-2023, Frustra Software [Jacob Wirth, Justin Li]  
All rights reserved.
