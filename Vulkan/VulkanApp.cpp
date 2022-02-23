#include "VulkanApp.h"
#ifdef _DEBUG
#include <cassert>
#else
#define assert(X) (void)(X);
#endif
#include <fstream>

using namespace vk;

LRESULT VulkanApp::WndProcAlloter(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_NCCREATE) {
		CREATESTRUCT* ps = (CREATESTRUCT*)lParam;
		VulkanApp* wnd = (VulkanApp*)ps->lpCreateParams;
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)wnd);
	}
	VulkanApp* wnd = (VulkanApp*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	return wnd == nullptr ? DefWindowProc(hwnd, msg, wParam, lParam) : wnd->WndProc(hwnd, msg, wParam, lParam);
}

ShaderModule VulkanApp::CreateShaderModule(const std::vector<char>& code)
{
	ShaderModuleCreateInfo createInfo{};
	createInfo.sType = StructureType::eShaderModuleCreateInfo;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
	ShaderModule shaderModule{};
	assert(device.createShaderModule(&createInfo, nullptr, &shaderModule) == Result::eSuccess);
	return shaderModule;
}

uint32_t VulkanApp::FindMemoryType(uint32_t typeFilter, MemoryPropertyFlags properties)
{
	PhysicalDeviceMemoryProperties memProperties;
	physicalDevice.getMemoryProperties(&memProperties);
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if (typeFilter & 1 << i && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
	throw std::runtime_error("failed to find suitable memory type!");
}

void VulkanApp::DrawFrame()
{
	device.waitForFences(1, &fences[currentFrame], VK_TRUE, UINT64_MAX);
	SubmitInfo s{};
	s.sType = StructureType::eSubmitInfo;
	s.signalSemaphoreCount = 1;
	s.pSignalSemaphores = &imageSemaphores[currentFrame];
	queue.submit(1, &s, Fence(nullptr));
	uint32_t imageIndex = pSwapchain->GetCurrentBackBufferIndex();
	if (imageFences[imageIndex] != Fence(VK_NULL_HANDLE)) {
		device.waitForFences(1, &imageFences[imageIndex], VK_TRUE, UINT64_MAX);
	}
	imageFences[imageIndex] = fences[currentFrame];
	SubmitInfo submitInfo{};
	submitInfo.sType = StructureType::eSubmitInfo;
	Semaphore waitSemaphores[] = { imageSemaphores[currentFrame] };
	PipelineStageFlags waitStages[] = { PipelineStageFlagBits::eColorAttachmentOutput };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
	Semaphore signalSemaphores[] = { renderSemaphores[currentFrame] };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;
	device.resetFences(1, &fences[currentFrame]);
	assert(queue.submit(1, &submitInfo, fences[currentFrame]) == Result::eSuccess);
	assert(imageIndex == pSwapchain->GetCurrentBackBufferIndex());
	FenceCreateInfo fci{};
	fci.sType = StructureType::eFenceCreateInfo;
	Fence f;
	device.createFence(&fci, nullptr, &f);
	PipelineStageFlags waitStage = PipelineStageFlagBits::eBottomOfPipe;
	SubmitInfo sub{};
	sub.sType = StructureType::eSubmitInfo;
	sub.waitSemaphoreCount = 1;
	sub.pWaitSemaphores = &renderSemaphores[imageIndex];
	sub.pWaitDstStageMask = &waitStage;
	assert(queue.submit(1, &sub, f) == Result::eSuccess);
	assert(device.waitForFences(1, &f, VK_TRUE, UINT64_MAX) == Result::eSuccess);
	device.destroyFence(f, nullptr);
	pSwapchain->Present(1, 0);
	currentFrame = (currentFrame + 1) % MaxFrame;
}

#ifdef _DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
	if (messageType & (VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT))
	{
		MessageBoxA(nullptr, pCallbackData->pMessage, "validation layer ", MB_ICONERROR);
	}
	return VK_FALSE;
}
#endif

VulkanApp::VulkanApp() : hwnd(CreateWindowEx(0, WndClsName, L"vulkan", WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX | WS_SIZEBOX), 200, 200, 1280, 720, nullptr, nullptr, nullptr, this))
{
	if (hwnd == nullptr) throw std::exception("null pointer exception");
#pragma region CreateInstance
	{
		ApplicationInfo appInfo{};
		appInfo.sType = StructureType::eApplicationInfo;
		appInfo.pNext = nullptr;
		appInfo.pApplicationName = "vulkan";
		appInfo.pEngineName = "hacker";
		appInfo.apiVersion = VK_MAKE_VERSION(1, 2, 198);
		InstanceCreateInfo createInfo{};
		createInfo.sType = StructureType::eInstanceCreateInfo;
		createInfo.pNext = nullptr;
		createInfo.pApplicationInfo = &appInfo;
		std::vector<const char*> enabledExtensions{
#ifdef _DEBUG
			VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
			VK_KHR_SURFACE_EXTENSION_NAME,
			VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
			VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
			VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
		};
		createInfo.enabledExtensionCount = enabledExtensions.size();
		createInfo.ppEnabledExtensionNames = enabledExtensions.data();
		auto result = createInstance(&createInfo, nullptr, &instance);
		assert(result == Result::eSuccess);
		void LoadReferences(VkInstance instance);
		LoadReferences(instance);
	}
#pragma endregion
#pragma region SetupDebug
#ifdef _DEBUG
	{
		DebugUtilsMessengerCreateInfoEXT createInfo{};
		createInfo.sType = StructureType::eDebugUtilsMessengerCreateInfoEXT;
		createInfo.messageSeverity = DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | DebugUtilsMessageSeverityFlagBitsEXT::eError | DebugUtilsMessageSeverityFlagBitsEXT::eWarning;
		createInfo.messageType = DebugUtilsMessageTypeFlagBitsEXT::eGeneral | DebugUtilsMessageTypeFlagBitsEXT::eValidation | DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
		createInfo.pfnUserCallback = DebugCallback;
		debugger = instance.createDebugUtilsMessengerEXT(createInfo, nullptr);
	}
#endif
#pragma endregion
#pragma region CreateDevice
	{
		uint32_t deviceCount = 0;
		assert(instance.enumeratePhysicalDevices(&deviceCount, nullptr) == Result::eSuccess);
		assert(deviceCount >= 1);
		std::vector<PhysicalDevice> physicalDevices(deviceCount);
		assert(instance.enumeratePhysicalDevices(&deviceCount, physicalDevices.data()) == Result::eSuccess);
		physicalDevice = physicalDevices[0];
		float priorities[] = { 1.0f };
		DeviceQueueCreateInfo queueInfo{};
		queueInfo.sType = StructureType::eDeviceQueueCreateInfo;
		queueInfo.pNext = nullptr;
		queueInfo.queueFamilyIndex = 0;
		queueInfo.queueCount = 1;
		queueInfo.pQueuePriorities = priorities;
		std::vector<const char*> enabledExtensions{
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
			VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
			VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
			VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
			VK_KHR_BIND_MEMORY_2_EXTENSION_NAME
		};
		DeviceCreateInfo deviceInfo{};
		deviceInfo.sType = StructureType::eDeviceCreateInfo;
		deviceInfo.pNext = nullptr;
		deviceInfo.queueCreateInfoCount = 1;
		deviceInfo.pQueueCreateInfos = &queueInfo;
		deviceInfo.enabledExtensionCount = enabledExtensions.size();
		deviceInfo.ppEnabledExtensionNames = enabledExtensions.data();
		deviceInfo.pEnabledFeatures = nullptr;
		assert(physicalDevice.createDevice(&deviceInfo, nullptr, &device) == Result::eSuccess);
		device.getQueue(0, 0, &queue);
	}
#pragma endregion
#pragma region CreateSwapchain
	Extent2D swapchainExtent{};
	{
		Win32SurfaceCreateInfoKHR surfaceCreateInfo{};
		surfaceCreateInfo.sType = StructureType::eWin32SurfaceCreateInfoKHR;
		surfaceCreateInfo.pNext = nullptr;
		surfaceCreateInfo.hinstance = GetModuleHandle(nullptr);
		surfaceCreateInfo.hwnd = hwnd;
		auto result = instance.createWin32SurfaceKHR(&surfaceCreateInfo, nullptr, &surface);
		assert(result == Result::eSuccess);
	}
	{
		SurfaceCapabilitiesKHR caps{};
		auto result = physicalDevice.getSurfaceCapabilitiesKHR(surface, &caps);
		assert(result == Result::eSuccess);
		if (caps.currentExtent.width == -1 || caps.currentExtent.height == -1) {
			swapchainExtent.width = 1280;
			swapchainExtent.height = 720;
		}
		else {
			swapchainExtent = caps.currentExtent;
		}
		CreateDXGIFactory2(0, IID_PPV_ARGS(&pFactory));
		PhysicalDeviceIDPropertiesKHR p{};
		p.sType = StructureType::ePhysicalDeviceIdPropertiesKHR;
		PhysicalDeviceProperties2KHR p2{};
		p2.sType = StructureType::ePhysicalDeviceProperties2KHR;
		p2.pNext = &p;
		physicalDevice.getProperties2KHR(&p2);
		if (p.deviceLUIDValid == false) throw std::exception("unknow exception");
		{
			auto l = reinterpret_cast<LUID*>(&p.deviceLUID);
			IDXGIAdapter4* adp;
			pFactory->EnumAdapterByLuid(*l, IID_PPV_ARGS(&adp));
			D3D12CreateDevice(adp, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&pD3d12Device));
			adp->Release();
		}
		const auto nodeCount = pD3d12Device->GetNodeCount();
		const UINT nodeMask = nodeCount <= 1 ? 0 : p.deviceNodeMask;
		const D3D12_COMMAND_QUEUE_DESC desc{ D3D12_COMMAND_LIST_TYPE_DIRECT,D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,D3D12_COMMAND_QUEUE_FLAG_NONE,nodeMask };
		pD3d12Device->CreateCommandQueue(&desc, IID_PPV_ARGS(&pCommandQueue));
		uint32_t imageCount = 2;
		DXGI_SWAP_CHAIN_DESC1 swapchainDesc{};
		swapchainDesc.Width = swapchainExtent.width;
		swapchainDesc.Height = swapchainExtent.height;
		swapchainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		swapchainDesc.SampleDesc.Count = 1;
		swapchainDesc.SampleDesc.Quality = 0;
		swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapchainDesc.BufferCount = imageCount;
		swapchainDesc.Scaling = DXGI_SCALING_NONE;
		swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
		IDXGISwapChain1* sp1;
		pFactory->CreateSwapChainForHwnd(pCommandQueue, hwnd, &swapchainDesc, nullptr, nullptr, &sp1);
		sp1->QueryInterface(IID_PPV_ARGS(&pSwapchain));
		sp1->Release();
		pFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
		backBuffers.resize(imageCount);
		for (UINT i = 0; i < imageCount; ++i) {
			pSwapchain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i].resource));
			auto r = backBuffers[i].resource;
			auto d = r->GetDesc();
			D3D12_HEAP_PROPERTIES hp;
			D3D12_HEAP_FLAGS hf;
			r->GetHeapProperties(&hp, &hf);
			ExternalMemoryImageCreateInfoKHR ef{};
			ef.sType = StructureType::eExternalMemoryImageCreateInfoKHR;
			ef.handleTypes = ExternalMemoryHandleTypeFlagBits::eD3D12Resource;
			ImageCreateInfo imageci{};
			imageci.sType = StructureType::eImageCreateInfo;
			imageci.imageType = ImageType::e2D;
			imageci.format = Format::eB8G8R8A8Unorm;
			imageci.extent.width = d.Width;
			imageci.extent.height = d.Height;
			imageci.extent.depth = 1;
			imageci.mipLevels = 1;
			imageci.arrayLayers = 1;
			imageci.samples = SampleCountFlagBits::e1;
			imageci.tiling = ImageTiling::eOptimal;
			imageci.usage = ImageUsageFlagBits::eColorAttachment;
			imageci.sharingMode = SharingMode::eExclusive;
			imageci.initialLayout = ImageLayout::eUndefined;
			device.createImage(&imageci, nullptr, &backBuffers[i].image);
			pD3d12Device->CreateSharedHandle(r, nullptr, GENERIC_ALL, nullptr, &backBuffers[i].handle);
			MemoryWin32HandlePropertiesKHR w32MemProps{};
			w32MemProps.sType = StructureType::eMemoryWin32HandlePropertiesKHR;
			w32MemProps.memoryTypeBits = 0xcdcdcdcd;
			assert(device.getMemoryWin32HandlePropertiesKHR(ExternalMemoryHandleTypeFlagBits::eD3D12Resource, backBuffers[i].handle, &w32MemProps) == Result::eSuccess);
			MemoryRequirements memReq;
			device.getImageMemoryRequirements(backBuffers[i].image, &memReq);
			if (w32MemProps.memoryTypeBits == 0xcdcdcdcd) w32MemProps.memoryTypeBits = memReq.memoryTypeBits;
			else w32MemProps.memoryTypeBits &= memReq.memoryTypeBits;
			auto memTypeIndex = FindMemoryType(w32MemProps.memoryTypeBits, MemoryPropertyFlagBits::eDeviceLocal);
			MemoryDedicatedAllocateInfoKHR dedicatedAllocateInfo{};
			dedicatedAllocateInfo.sType = StructureType::eMemoryDedicatedAllocateInfoKHR;
			dedicatedAllocateInfo.image = backBuffers[i].image;
			ImportMemoryWin32HandleInfoKHR importMemoryInfo{};
			importMemoryInfo.sType = StructureType::eImportMemoryWin32HandleInfoKHR;
			importMemoryInfo.pNext = &dedicatedAllocateInfo;
			importMemoryInfo.handleType = ExternalMemoryHandleTypeFlagBits::eD3D12Resource;
			importMemoryInfo.handle = backBuffers[i].handle;
			MemoryAllocateInfo memoryAllocateInfo{};
			memoryAllocateInfo.sType = StructureType::eMemoryAllocateInfo;
			memoryAllocateInfo.pNext = &importMemoryInfo;
			memoryAllocateInfo.allocationSize = memReq.size;
			memoryAllocateInfo.memoryTypeIndex = memTypeIndex;
			assert(device.allocateMemory(&memoryAllocateInfo, nullptr, &backBuffers[i].memory) == Result::eSuccess);
			BindImageMemoryInfoKHR bind{};
			bind.sType = StructureType::eBindImageMemoryInfoKHR;
			bind.image = backBuffers[i].image;
			bind.memory = backBuffers[i].memory;
			bind.memoryOffset = 0;
			if (device.bindImageMemory2KHR(1, &bind) != Result::eSuccess) throw std::exception("unknow exception");
			ImageViewCreateInfo createInfo{};
			createInfo.sType = StructureType::eImageViewCreateInfo;
			createInfo.image = backBuffers[i].image;
			createInfo.viewType = ImageViewType::e2D;
			createInfo.format = Format::eB8G8R8A8Unorm;
			createInfo.components.r = ComponentSwizzle::eIdentity;
			createInfo.components.g = ComponentSwizzle::eIdentity;
			createInfo.components.b = ComponentSwizzle::eIdentity;
			createInfo.components.a = ComponentSwizzle::eIdentity;
			createInfo.subresourceRange.aspectMask = ImageAspectFlagBits::eColor;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;
			result = device.createImageView(&createInfo, nullptr, &backBuffers[i].view);
			assert(result == Result::eSuccess);
		}
	}
#pragma endregion
#pragma region CreateRenderPass
	{
		AttachmentDescription colorAttachment{};
		colorAttachment.format = Format::eR8G8B8A8Unorm;
		colorAttachment.samples = SampleCountFlagBits::e1;
		colorAttachment.loadOp = AttachmentLoadOp::eClear;
		colorAttachment.storeOp = AttachmentStoreOp::eStore;
		colorAttachment.stencilLoadOp = AttachmentLoadOp::eDontCare;
		colorAttachment.stencilStoreOp = AttachmentStoreOp::eDontCare;
		colorAttachment.initialLayout = ImageLayout::eUndefined;
		colorAttachment.finalLayout = ImageLayout::ePresentSrcKHR;
		AttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = ImageLayout::eColorAttachmentOptimal;
		SubpassDescription subpass{};
		subpass.pipelineBindPoint = PipelineBindPoint::eGraphics;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		SubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = PipelineStageFlagBits::eColorAttachmentOutput;
		dependency.srcAccessMask = AccessFlagBits::eNoneKHR;
		dependency.dstStageMask = PipelineStageFlagBits::eColorAttachmentOutput;
		dependency.dstAccessMask = AccessFlagBits::eColorAttachmentWrite;
		RenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = StructureType::eRenderPassCreateInfo;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;
		assert(device.createRenderPass(&renderPassInfo, nullptr, &renderPass) == Result::eSuccess);
	}
#pragma endregion
#pragma region CreatePipeline
	{
		auto&& vertexShaderCode = ReadFile("vs.spv");
		auto&& pixelShaderCode = ReadFile("ps.spv");
		auto&& vertexShaderModule = CreateShaderModule(vertexShaderCode);
		auto&& pixelShaderModule = CreateShaderModule(pixelShaderCode);
		PipelineShaderStageCreateInfo vertexShaderStageInfo{};
		vertexShaderStageInfo.sType = StructureType::ePipelineShaderStageCreateInfo;
		vertexShaderStageInfo.stage = ShaderStageFlagBits::eVertex;
		vertexShaderStageInfo.module = vertexShaderModule;
		vertexShaderStageInfo.pName = "main";
		PipelineShaderStageCreateInfo pixelShaderStageInfo{};
		pixelShaderStageInfo.sType = StructureType::ePipelineShaderStageCreateInfo;
		pixelShaderStageInfo.stage = ShaderStageFlagBits::eFragment;
		pixelShaderStageInfo.module = pixelShaderModule;
		pixelShaderStageInfo.pName = "main";
		PipelineShaderStageCreateInfo shaderStages[] = { vertexShaderStageInfo, pixelShaderStageInfo };
		auto bindingDescription = Vertex::GetBindingDescription();
		auto attributeDescriptions = Vertex::GetAttributeDescriptions();
		PipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = StructureType::ePipelineVertexInputStateCreateInfo;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
		PipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = StructureType::ePipelineInputAssemblyStateCreateInfo;
		inputAssembly.topology = PrimitiveTopology::eTriangleList;
		inputAssembly.primitiveRestartEnable = VK_FALSE;
		Viewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)swapchainExtent.width;
		viewport.height = (float)swapchainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		Rect2D scissor{};
		scissor.offset = { { 0, 0 } };
		scissor.extent = swapchainExtent;
		PipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = StructureType::ePipelineViewportStateCreateInfo;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;
		PipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = StructureType::ePipelineRasterizationStateCreateInfo;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = PolygonMode::eFill;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = CullModeFlagBits::eBack;
		rasterizer.frontFace = FrontFace::eClockwise;
		rasterizer.depthBiasEnable = VK_FALSE;
		PipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = StructureType::ePipelineMultisampleStateCreateInfo;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = SampleCountFlagBits::e1;
		PipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = ColorComponentFlagBits::eR | ColorComponentFlagBits::eG | ColorComponentFlagBits::eB | ColorComponentFlagBits::eA;
		colorBlendAttachment.blendEnable = VK_FALSE;
		PipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = StructureType::ePipelineColorBlendStateCreateInfo;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = LogicOp::eCopy;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f;
		colorBlending.blendConstants[1] = 0.0f;
		colorBlending.blendConstants[2] = 0.0f;
		colorBlending.blendConstants[3] = 0.0f;
		PipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = StructureType::ePipelineLayoutCreateInfo;
		pipelineLayoutInfo.setLayoutCount = 0;
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		assert(device.createPipelineLayout(&pipelineLayoutInfo, nullptr, &pipelineLayout) == Result::eSuccess);
		GraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = StructureType::eGraphicsPipelineCreateInfo;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.layout = pipelineLayout;
		pipelineInfo.renderPass = renderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		assert(device.createGraphicsPipelines(VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) == Result::eSuccess);
		device.destroyShaderModule(pixelShaderModule, nullptr);
		device.destroyShaderModule(vertexShaderModule, nullptr);
	}
#pragma endregion
#pragma region CreateOthers
	{
		framebuffers.resize(backBuffers.size());
		for (size_t i = 0; i < backBuffers.size(); i++) {
			ImageView attachments[] = { backBuffers[i].view };
			FramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = StructureType::eFramebufferCreateInfo;
			framebufferInfo.renderPass = renderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = swapchainExtent.width;
			framebufferInfo.height = swapchainExtent.height;
			framebufferInfo.layers = 1;
			assert(device.createFramebuffer(&framebufferInfo, nullptr, &framebuffers[i]) == Result::eSuccess);
		}
	}
	{
		CommandPoolCreateInfo poolInfo{};
		poolInfo.sType = StructureType::eCommandPoolCreateInfo;
		poolInfo.queueFamilyIndex = 0;
		assert(device.createCommandPool(&poolInfo, nullptr, &commandPool) == Result::eSuccess);
	}
	{
		BufferCreateInfo bufferInfo{};
		bufferInfo.sType = StructureType::eBufferCreateInfo;
		bufferInfo.size = sizeof(Vertex) * vertices.size();
		bufferInfo.usage = BufferUsageFlagBits::eVertexBuffer;
		bufferInfo.sharingMode = SharingMode::eExclusive;
		assert(device.createBuffer(&bufferInfo, nullptr, &vertexBuffer) == Result::eSuccess);
		MemoryRequirements memRequirements;
		device.getBufferMemoryRequirements(vertexBuffer, &memRequirements);
		MemoryAllocateInfo allocInfo{};
		allocInfo.sType = StructureType::eMemoryAllocateInfo;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, MemoryPropertyFlagBits::eHostVisible | MemoryPropertyFlagBits::eHostCoherent);
		assert(device.allocateMemory(&allocInfo, nullptr, &vertexBufferMemory) == Result::eSuccess);
		device.bindBufferMemory(vertexBuffer, vertexBufferMemory, 0);
		void* data = device.mapMemory(vertexBufferMemory, 0, bufferInfo.size);
		memcpy(data, vertices.data(), bufferInfo.size);
		device.unmapMemory(vertexBufferMemory);
	}
	{
		commandBuffers.resize(framebuffers.size());
		CommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = StructureType::eCommandBufferAllocateInfo;
		allocInfo.commandPool = commandPool;
		allocInfo.level = CommandBufferLevel::ePrimary;
		allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();
		assert(device.allocateCommandBuffers(&allocInfo, commandBuffers.data()) == Result::eSuccess);
		for (size_t i = 0; i < commandBuffers.size(); i++) {
			CommandBufferBeginInfo beginInfo{};
			beginInfo.sType = StructureType::eCommandBufferBeginInfo;
			assert(commandBuffers[i].begin(&beginInfo) == Result::eSuccess);
			RenderPassBeginInfo renderPassInfo{};
			renderPassInfo.sType = StructureType::eRenderPassBeginInfo;
			renderPassInfo.renderPass = renderPass;
			renderPassInfo.framebuffer = framebuffers[i];
			renderPassInfo.renderArea.offset = { {0, 0} };
			renderPassInfo.renderArea.extent = swapchainExtent;
			ClearValue clearColor(ClearColorValue(std::array<float, 4>{0.0f, 1.0f, 1.0f, 1.0f}));
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearColor;
			Buffer vertexBuffers[] = { vertexBuffer };
			DeviceSize offsets[] = { 0 };
			commandBuffers[i].beginRenderPass(&renderPassInfo, SubpassContents::eInline);
			commandBuffers[i].bindPipeline(PipelineBindPoint::eGraphics, graphicsPipeline);
			commandBuffers[i].bindVertexBuffers(0, 1, vertexBuffers, offsets);
			commandBuffers[i].draw(3, 1, 0, 0);
			commandBuffers[i].endRenderPass();
			commandBuffers[i].end();
		}
	}
#pragma endregion
#pragma region CreateSyncObjects
	{
		imageSemaphores.resize(MaxFrame);
		renderSemaphores.resize(MaxFrame);
		fences.resize(MaxFrame);
		imageFences.resize(backBuffers.size(), VK_NULL_HANDLE);
		SemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = StructureType::eSemaphoreCreateInfo;
		FenceCreateInfo fenceInfo{};
		fenceInfo.sType = StructureType::eFenceCreateInfo;
		fenceInfo.flags = FenceCreateFlagBits::eSignaled;
		for (size_t i = 0; i < MaxFrame; i++) {
			assert(device.createSemaphore(&semaphoreInfo, nullptr, &imageSemaphores[i]) == Result::eSuccess);
			assert(device.createSemaphore(&semaphoreInfo, nullptr, &renderSemaphores[i]) == Result::eSuccess);
			assert(device.createFence(&fenceInfo, nullptr, &fences[i]) == Result::eSuccess);
		}
	}
#pragma endregion
}

VertexInputBindingDescription Vertex::GetBindingDescription()
{
	VertexInputBindingDescription bindingDescription{};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(Vertex);
	bindingDescription.inputRate = VertexInputRate::eVertex;
	return bindingDescription;
}

std::array<VertexInputAttributeDescription, 2> Vertex::GetAttributeDescriptions()
{
	std::array<VertexInputAttributeDescription, 2> attributeDescriptions{};
	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = Format::eR32G32B32A32Sfloat;
	attributeDescriptions[0].offset = offsetof(Vertex, pos);
	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = Format::eR32G32B32A32Sfloat;
	attributeDescriptions[1].offset = offsetof(Vertex, color);
	return attributeDescriptions;
}

std::vector<char> VulkanApp::ReadFile(const std::string& filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	if (!file.is_open()) throw std::exception("failed to open file!");
	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();
	return buffer;
}

LRESULT VulkanApp::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT:
		DrawFrame();
		return 0;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
}

int VulkanApp::Run()
{
	ShowWindow(hwnd, SW_SHOW);
	MSG msg;
	while (GetMessage(&msg, hwnd, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return (int)msg.wParam;
}

VulkanApp::~VulkanApp()
{
	for (size_t i = 0; i < 2; i++) {
		device.destroySemaphore(renderSemaphores[i], nullptr);
		device.destroySemaphore(imageSemaphores[i], nullptr);
		device.destroyFence(fences[i], nullptr);
	}
	for (auto buff : framebuffers) device.destroyFramebuffer(buff, nullptr);
	for (auto buff : backBuffers) {
		device.destroyImageView(buff.view, nullptr);
		device.destroyImage(buff.image, nullptr);
		device.freeMemory(buff.memory, nullptr);
		CloseHandle(buff.handle);
		buff.resource->Release();
	}
	device.freeCommandBuffers(commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
	device.destroyPipeline(graphicsPipeline, nullptr);
	device.destroyPipelineLayout(pipelineLayout, nullptr);
	device.destroyRenderPass(renderPass, nullptr);
	device.destroyCommandPool(commandPool, nullptr);
	pSwapchain->Release();
	pCommandQueue->Release();
	pD3d12Device->Release();
	pFactory->Release();
	device.destroy(nullptr);
	instance.destroySurfaceKHR(surface, nullptr);
#ifdef _DEBUG
	instance.destroyDebugUtilsMessengerEXT(debugger, nullptr);
#endif
	instance.destroy();
}
