#include <vulkan/vulkan_core.h>

extern "C" bool VulkanDynamicLink(void*, VkInstance);

void LoadReferences(VkInstance instance)
{
	if (!VulkanDynamicLink(vkGetInstanceProcAddr, instance)) throw "ex";
}
