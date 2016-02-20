Stray Photons
=============

Build setup
-----------

### Install Vulkan-capable graphics drivers

- NVIDIA: https://developer.nvidia.com/vulkan-driver
- AMD: http://gpuopen.com/gaming-product/vulkan/ (link is on the page somewhere)
- Intel (Linux only right now): available in upstream packages

### Install the LunarG Vulkan SDK

- Linux: https://vulkan.lunarg.com/pub/sdks/linux/latest
- Windows: https://vulkan.lunarg.com/pub/sdks/windows/latest

### Install build dependencies

- Linux: `cmake build-essential xorg-dev libxrandr-dev astyle mesa-common-dev` (or equivalent for your distro).
There might be some missing dependencies in this list, add any you find.

- Windows: cmake, Visual Studio (version TBD), and more.
We'll set something up with git bash to be able to run our scripts.

All: `make dependencies`

### Build

Linux: `make`

Windows: `make windows`, open generated sln and build in Visual Studio
