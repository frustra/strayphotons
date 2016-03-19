
#define VULKAN_ENABLE_VALIDATION

#define GLFW_INCLUDE_VULKAN
#define VKCPP_ENHANCED_MODE

#include <GLFW/glfw3.h>
#include <vulkan/vk_cpp.h>

#define GLM_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

static vk::AllocationCallbacks &nalloc = vk::AllocationCallbacks::null();

#endif