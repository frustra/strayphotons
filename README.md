# ![Stray Photons](https://assets.strayphotons.net/logos/sp-laser-black.svg) [![Build status](https://badge.buildkite.com/6ad6424eb4ac47ecf0738dfa96d3f011019a39d7b6066c363e.svg?branch=master)](https://buildkite.com/frustra/strayphotons)

Stray Photons is a high performance game engine with a focus on immersive VR experiences. Featuring full-hand physics interaction, seamless asynchronous scene Loading, fully interactive global illumination lighting, powerful game logic and scripting capabilities, and much more.

## Getting Started

For a guide on getting started with Stray Photons, check out the docs:  
https://docs.strayphotons.net/ or on [GitHub](https://github.com/frustra/strayphotons/tree/master/docs)

## Features

- **High Performance Core Engine** - Stray Photons is built on top of [Tecs](https://github.com/xthexder/Tecs), a transactional Entity Component System designed for multi-threaded applications. The core engine is designed to maximize framerate stability for VR, and take advantage of increasing core counts in modern processors.

- **VR-focused Design** - The engine's features are all designed with VR as the main focus. Interactions are all done through physics actors in the world such as the player's hands. Stray Photon's rendering has been designed around VR's high framerates and resolution requirements.

- **Voxel-Based GI Lighting** - Stray Photons supports fully interactive Global Illumination (GI) without the requirement of raytracing hardware. This voxel-based approach to GI lighting partially decouples the lighting calculation from the rendering resolution, allowing for interactive bounce lighting, even in VR.

    <img src="https://assets.strayphotons.net/demos/glowing_duck.png" alt="Glowing Duck Lighting" height="340" />

- **Robust Force-limited Physics System** - Based on Nvidia's PhysX, Stray Photons physics system includes custom fine-tuned constraints and controllers for handling VR hand input. Forces are accurately limited to prevent moving objects that are too heavy, or the player clipping out of bounds. Full-hand physics interaction is supported for precise player input.

    <img src="https://assets.strayphotons.net/demos/hand_physics.gif" alt="Accurate full-hand physics simulation" height="340" />

- **Light Reflections and Transparency** - The engine's lighting includes a custom recursive shadow system that allows direct lights to both bounce off mirrors, or be tinted by glass filters. Objects in a light's path can be moved in real-time by the player, allowing light-based puzzles to be created.

    <img src="https://assets.strayphotons.net/demos/glass_shadows.png" alt="Tinted Glass Shadows" height="340" />

- **Laser Optics System** - As part of Stray Photon's game logic, signals can be directly connected to laser emitters and sensors. The path of lasers can be manipulated in a variety of ways by combining blocks such as mirrors, splitters, gates, and charge cells (batteries). Laser optics can be combined by the player to create logic gates and other complex circuits.

    <img src="https://assets.strayphotons.net/demos/d_flip_flop.png" alt="Laser-Based D-flip-flop" height="340" />

- **Simple JSON-baased Scene Format** - Scenes in Stray Photons are defined using a human-readable (and writable) JSON format. A scene defines a list of entity definitions, including support for templates and other scriptable prefabs such as tiling or wall generation.

    <img src="https://assets.strayphotons.net/demos/json_schema_completion.png" alt="JSON Schema Auto-completion" height="340" />

- **Fully Asynchronous Scene Loading** - The scene manager loads scenes in the background using a staging environment before the completed scene is atomically added to the live world. Multiple scenes can be connected together to seamlessly load in and out as the player moves through the world without any loading screens.

- **Powerful Signal Expression System** - Without the need for writing any code at all, complex game logic can be represented using just signals and events sent between entities. A set of built-in scripts can be combined to control almost any engine component.

    <img src="https://assets.strayphotons.net/demos/game_of_life.gif" alt="Game of Life built using Signal Expressions" height="340" />

- **CI Testing Environment and Dev-Friendly Build Tools** - Stray Photon's development tools are made to keep iteration fast and easy. The engine's modularity allows for fast incremental builds, and flexible configurations for CI testing. Most aspects of the engine are tested, including automatic screenshot diffing of scripted scenarios. These test scripts allow the engine to be run in a deterministic "lockstep" mode for tracking down even single-frame issues.

    <img src="https://assets.strayphotons.net/demos/screenshot_diffs.png" alt="Buildkite Screenshot Diffs" height="340" />


## Dependencies

- Ensure graphics drivers are up to date.
- Vulkan >= 1.2 is required. Get the SDK from https://www.vulkan.org/tools.
- CMake and a C++20 compiler (Windows: MSVC, Linux: Clang or GCC).
- Git submodules must be checked out using `git submodule update --init --recursive`
- Python is used for CI automation
- Rust is optional for building the test linked library
- VSCode is recommended, most of the workspace config is included

## License

> Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
> 
> Stray Photons is licensed under the terms of the [Mozilla Public License Version 2.0](https://mozilla.org/MPL/2.0/).  
> Dependencies are licensed under individual MPL2.0-compatible licenses.  
> See [ADDITIONAL_LICENSES.md](https://github.com/frustra/strayphotons/blob/master/ADDITIONAL_LICENSES.md) for details.
