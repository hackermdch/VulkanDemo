#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <string>
#include <Windows.h>
#include <vulkan/vulkan.hpp>
#include <vector>
#include <array>
#include <DirectXMath.h>
#include <d3d12.h>
#include <dxgi1_6.h>

struct Vertex
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT4 color;
	static vk::VertexInputBindingDescription GetBindingDescription();
	static std::array<vk::VertexInputAttributeDescription, 2> GetAttributeDescriptions();
};

struct BackBuffer
{
	HANDLE handle;
	ID3D12Resource* resource;
	vk::Image image;
	vk::ImageView view;
	vk::DeviceMemory memory;
};

class VulkanApp
{
#ifdef _DEBUG
	vk::DebugUtilsMessengerEXT debugger;
#endif
private:
	constexpr static uint32_t MaxFrame = 2;
	IDXGIFactory7* pFactory;
	ID3D12Device* pD3d12Device;
	ID3D12CommandQueue* pCommandQueue;
	IDXGISwapChain4* pSwapchain;
	const HWND hwnd;
	vk::Instance instance;
	vk::PhysicalDevice physicalDevice;
	vk::Device device;
	vk::Queue queue;
	vk::SurfaceKHR surface;
	vk::RenderPass renderPass;
	vk::PipelineLayout pipelineLayout;
	vk::Pipeline graphicsPipeline;
	vk::CommandPool commandPool;
	vk::Buffer vertexBuffer;
	vk::DeviceMemory vertexBufferMemory;
	std::vector<BackBuffer> backBuffers;
	std::vector<vk::Framebuffer> framebuffers;
	std::vector<vk::CommandBuffer> commandBuffers;
	std::vector<vk::Semaphore> imageSemaphores;
	std::vector<vk::Semaphore> renderSemaphores;
	std::vector<vk::Fence> fences;
	std::vector<vk::Fence> imageFences;
	uint32_t currentFrame;
public:
	static const wchar_t* WndClsName;
	const std::vector<Vertex> vertices = {
	{{0.0f, -0.5f, 0}, {1.0f, 0.0f, 0.0f, 1}},
	{{0.5f, 0.5f, 0}, {0.0f, 1.0f, 0.0f, 1}},
	{{-0.5f, 0.5f, 0}, {0.0f, 0.0f, 1.0f, 1}}
	};
private:
	static std::vector<char> ReadFile(const std::string& filename);
	virtual LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	vk::ShaderModule CreateShaderModule(const std::vector<char>& code);
	uint32_t FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
	void DrawFrame();
public:
	static LRESULT WndProcAlloter(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	int Run();
	VulkanApp();
	virtual ~VulkanApp();
};
