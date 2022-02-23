#include <Windows.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_win32.h>

#define NOINLINE __declspec(noinline)

#pragma optimize("g", off)
#ifdef _DEBUG
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
#endif

NOINLINE VKAPI_ATTR VkResult VKAPI_CALL vkBindImageMemory2KHR(
	VkDevice,
	uint32_t,
	const VkBindImageMemoryInfo*)
{
	return VK_SUCCESS;
}

NOINLINE VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceProperties2KHR(
	VkPhysicalDevice,
	VkPhysicalDeviceProperties2*)
{
	return (void)0;
}

NOINLINE VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryWin32HandlePropertiesKHR(
	VkDevice,
	VkExternalMemoryHandleTypeFlagBits,
	HANDLE,
	VkMemoryWin32HandlePropertiesKHR*)
{
	return VK_SUCCESS;
}
#pragma optimize("g", on)

void LoadReferences(VkInstance instance)
{
	void SetLink(void*, void*);
#ifdef _DEBUG
	SetLink(vkCreateDebugUtilsMessengerEXT, vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
	SetLink(vkDestroyDebugUtilsMessengerEXT, vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
#endif
	SetLink(vkGetPhysicalDeviceProperties2KHR, vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2KHR"));
	SetLink(vkGetMemoryWin32HandlePropertiesKHR, vkGetInstanceProcAddr(instance, "vkGetMemoryWin32HandlePropertiesKHR"));
	SetLink(vkBindImageMemory2KHR, vkGetInstanceProcAddr(instance, "vkBindImageMemory2KHR"));
}
