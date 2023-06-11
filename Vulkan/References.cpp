#include <vulkan/vulkan_core.h>

#define NOINLINE __declspec(noinline)

NOINLINE VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
	VkInstance,
	const VkDebugUtilsMessengerCreateInfoEXT*,
	const VkAllocationCallbacks*,
	VkDebugUtilsMessengerEXT*
)
{
	return VK_SUCCESS;
}

NOINLINE VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(
	VkInstance,
	VkDebugUtilsMessengerEXT,
	const VkAllocationCallbacks*
)
{
	return (void)0;
}

void LoadReferences(VkInstance instance)
{
	void SetLink(void*, void*);
	SetLink(vkCreateDebugUtilsMessengerEXT, vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
	SetLink(vkDestroyDebugUtilsMessengerEXT, vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
}