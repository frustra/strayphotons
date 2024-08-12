## Installation

### Dependencies

Stray Photons relies on several dependencies which must be installed manually:
- [Vulkan SDK](https://www.vulkan.org/tools) Version 1.2 or higher
- A C++20 compatible compiler (currently supported are MSVC 2019, MSVC 2022, Clang 12+, and GCC 11+)
- CMake Version 3.20 or higher
- Rust (optional for building the Rust module)
- Python 3 (used in the CI pipeline, but not necessary for development)
- VSCode is recommended for development, but building with CMake from the command line is also possible.

### Downloading the source code

The easiest way to getting Stray Photons and all of its dependencies is through Git:

```
~/dev$ git clone https://github.com/frustra/strayphotons.git
~/dev$ cd strayphotons
~/dev/strayphotons$
```

Next, make sure you have all the latest submodule dependencies:

```
~/dev/strayphotons$ git submodule update --init
```

From here you can open up the `strayphotons` directory in VSCode, or continue building via command line.

### Building from VSCode

The project repository already include a basic workspace configuration to make working on Stray Photons easier.

After installing the recommended VSCode extensions, a CMake build variant can be selected. There are a few settings to choose from:
- **Build Type**: Debug, RelWithDebInfo, Release  
  For most purposes RelWithDebInfo is recommended. Debug will have faster build times, but can have low framerates.
- **Package Type**: Developer Build, Package Build  
  Package Build bundles assets into a single file, and is meant to be suitable for distribution. Developer Build is recommended for most purposes.
- **Tracy Profiler**: On, Off  
  The Tracy C++ Profiler is a powerful live debugging tool, however it rapidly consumes ram making longer play sessions an issue. This setting can remove it from the build.
- **Rust Shared Library**: On, Off  
  Rust library linking is supported, however this can be disable if Rust is not installed on the system.

Once the build variant is selected, CMake Configure will usually run automatically, however you can also manually reconfigure the project using `>CMake: Delete Cache and Reconfigure` in the Ctrl-Shift-P menu.

CMake Configure will download all the required project assets such as GLTF models, textures, and audio files.

The project can then be built using `>CMake: Build` in the Ctrl-Shift-P menu or the `F7` hotkey

### Building from Command Line

CMake Configure should be run inside of the `build/` subfolder, which you may need to create yourself:
```
~/dev/strayphotons$ mkdir build/
~/dev/strayphotons$ cd build
~/dev/strayphotons/build$
```

The project can then be configured using the following command and an optional set of build flags:
```
~/dev/strayphotons/build$ cmake .. -GNinja <build option flags>
```

Build Options:
- `-DCMAKE_BUILD_TYPE=` Debug, RelWithDebInfo, or Release (default: Debug)
- `-DSP_PACKAGE_RELEASE=` 0 or 1 (default: 0)
- `-DSP_ENABLE_TRACY=` 0 or 1 (default: 1)
- `-DSP_BUILD_RUST=` 0 or 1 (default: 1)

Ninja Build is the recommended build system for Stray Photons. See VSCode cmake variants for more info on build options.

The configure step will automatically download all the required project assets such as GLTF models, textures, and audio files.

The project can then be built by running Ninja build:
```
~/dev/strayphotons/build$ ninja
```

## Running Stray Photons

The project will generate a number of executables, however in this guide we are only concerned with `sp-vk` and `sp-test`:
```
Stray Photons Game Engine

Usage:
  strayphotons [OPTION...]

  -h, --help                    Display help
  -r, --run arg                 Load commands from a file an execute them in the console
  -s, --scene arg               Initial scene to load
      --size width height       Initial window size
      --no-vr                   Disable automatic XR/VR system loading
      --headless                Disable window creation and graphics initialization
      --with-validation-layers  Enable Vulkan validation layers
  -c, --command arg             Run a console command on init
  -v, --verbose                 Enable debug logging
```

`sp-test` is a special test environment variant of `sp-vk` that runs windowless for automated testing on a headless server.
When a command script is provided via thw `--run` option, all threads run in lock-step, and must be manually advanced by
running the commands `stepgraphics [count]`, `stepphysics [count]`, and `steplogic [count]`.
Execution stops when the console execution queue is empty.
```
Stray Photons Game Engine Test Environment

Usage:
  sp-test [OPTION...] --run <script-file>

  -h, --help                    Display help
  -r, --run arg                 Load commands from a file an execute them in the console
  -s, --scene arg               Initial scene to load
      --size width height       Initial window size
      --headless                Disable window creation and graphics initialization
      --with-validation-layers  Enable Vulkan validation layers
  -c, --command arg             Run a console command on init
  -v, --verbose                 Enable debug logging
```

For both executables, the startup order is as follows:
1. `--command` commands are executed
1. If not `--headless`, Graphics is initialized using `--size` or 1080p by default
1. If not `--no-vr`, the XR Runtime is loaded
1. The `player` and `bindings` scenes are loaded
1. If in test mode, commands from `script-file` are queued in the console
1. If a `--scene` is provided, that scene is loaded, else the `menu` scene is loaded.

A number of pre-defined launch configurations are available through the provided VSCode debugger launch.json.

See [Loading Scenes](./Loading_Scenes.md) for more info on engine usage.

## Default Input Bindings

<table>
<tr><th>In-Game Bindings</th><th>System Bindings</th></tr>
<tr><td>

| Key           | Player Action         |
|---------------|-----------------------|
| Escape        | Open Pause Menu, Back |
| W             | Move Forward          |
| D             | Move Right            |
| A             | Move Left             |
| S             | Move Back             |
| Left Control  | Move Down             |
| Left Shift    | Sprint                |
| Space         | Jump                  |
| E             | Grab Item             |
| R             | Rotate Interaction    |
| F             | Toggle Flashlight     |
| P             | Place/Grab Flashlight |
| V             | Toggle Noclip         |
| Q             | Toggle Editor Tray    |

</td><td>

| Key          | Command Value                                         |
|--------------|-------------------------------------------------------|
| Backtick (`) | Toggle Console                                        |
| F1           | Toggle Editor (Inspects entity under crosshair)       |
| F2           | Open Tracy Profiler (only first instance can connect) |
| F5           | Reload current scene, preserving player position      |
| F6           | Reload current scene and reload player                |
| F7           | Reload Vulkan shaders                                 |

</td></tr> </table>
