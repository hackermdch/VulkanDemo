#include "VulkanApp.h"
#include <cassert>
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
	uint32_t imageIndex;
	auto result = device.acquireNextImageKHR(swapchain, UINT64_MAX, imageSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
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
	PresentInfoKHR presentInfo{};
	presentInfo.sType = StructureType::ePresentInfoKHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.pImageIndices = &imageIndex;
	result = queue.presentKHR(&presentInfo);
	assert(result == Result::eSuccess);
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
#ifdef NDEBUG
		std::vector<const char*> enabledExtensions{ VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
#else
		std::vector<const char*> enabledExtensions{ VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME, VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
#endif
		createInfo.enabledExtensionCount = enabledExtensions.size();
		createInfo.ppEnabledExtensionNames = enabledExtensions.data();
		auto result = createInstance(&createInfo, nullptr, &instance);
		assert(result == Result::eSuccess);
		void LoadReferences(VkInstance);
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
		auto result = instance.enumeratePhysicalDevices(&deviceCount, nullptr);
		assert(result == Result::eSuccess);
		assert(deviceCount >= 1);
		std::vector<PhysicalDevice> physicalDevices(deviceCount);
		result = instance.enumeratePhysicalDevices(&deviceCount, physicalDevices.data());
		assert(result == Result::eSuccess);
		physicalDevice = physicalDevices[0];
		float priorities[] = { 1.0f };
		DeviceQueueCreateInfo queueInfo{};
		queueInfo.sType = StructureType::eDeviceQueueCreateInfo;
		queueInfo.pNext = nullptr;
		queueInfo.queueFamilyIndex = 0;
		queueInfo.queueCount = 1;
		queueInfo.pQueuePriorities = priorities;
		std::vector<const char*> enabledExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
		DeviceCreateInfo deviceInfo{};
		deviceInfo.sType = StructureType::eDeviceCreateInfo;
		deviceInfo.pNext = nullptr;
		deviceInfo.queueCreateInfoCount = 1;
		deviceInfo.pQueueCreateInfos = &queueInfo;
		deviceInfo.enabledExtensionCount = enabledExtensions.size();
		deviceInfo.ppEnabledExtensionNames = enabledExtensions.data();
		deviceInfo.pEnabledFeatures = nullptr;
		result = physicalDevice.createDevice(&deviceInfo, nullptr, &device);
		assert(result == Result::eSuccess);
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
		uint32_t presentModeCount = 0;
		result = physicalDevice.getSurfacePresentModesKHR(surface, &presentModeCount, nullptr);
		assert(result == Result::eSuccess);
		assert(presentModeCount >= 1);
		std::vector<PresentModeKHR> presentModes(presentModeCount);
		result = physicalDevice.getSurfacePresentModesKHR(surface, &presentModeCount, presentModes.data());
		assert(result == Result::eSuccess);
		PresentModeKHR presentMode = (PresentModeKHR)-1;
		for (uint32_t i = 0; i < presentModeCount; i++) {
			if (presentModes[i] == PresentModeKHR::eImmediate) {
				presentMode = PresentModeKHR::eImmediate;
				break;
			}
		}
		if (presentMode == (PresentModeKHR)-1) throw std::exception("no present mode support");
		assert(caps.maxImageCount >= 1);
		uint32_t imageCount = 2;
		SwapchainCreateInfoKHR swapchainCreateInfo = {};
		swapchainCreateInfo.sType = StructureType::eSwapchainCreateInfoKHR;
		swapchainCreateInfo.surface = surface;
		swapchainCreateInfo.minImageCount = imageCount;
		swapchainCreateInfo.imageFormat = Format::eR8G8B8A8Unorm;
		swapchainCreateInfo.imageColorSpace = ColorSpaceKHR::eSrgbNonlinear;
		swapchainCreateInfo.imageExtent = swapchainExtent;
		swapchainCreateInfo.imageArrayLayers = 1;
		swapchainCreateInfo.imageUsage = ImageUsageFlagBits::eColorAttachment;
		swapchainCreateInfo.imageSharingMode = SharingMode::eExclusive;
		swapchainCreateInfo.queueFamilyIndexCount = 1;
		swapchainCreateInfo.pQueueFamilyIndices = { 0 };
		swapchainCreateInfo.preTransform = SurfaceTransformFlagBitsKHR::eIdentity;
		swapchainCreateInfo.compositeAlpha = CompositeAlphaFlagBitsKHR::eOpaque;
		swapchainCreateInfo.presentMode = presentMode;
		result = device.createSwapchainKHR(&swapchainCreateInfo, nullptr, &swapchain);
		assert(result == Result::eSuccess);
		result = device.getSwapchainImagesKHR(swapchain, &imageCount, nullptr);
		assert(result == Result::eSuccess);
		assert(imageCount > 0);
		images.resize(imageCount);
		result = device.getSwapchainImagesKHR(swapchain, &imageCount, images.data());
		assert(result == Result::eSuccess);
		views.resize(imageCount);
		for (auto i = 0; i < imageCount; i++) {
			ImageViewCreateInfo createInfo{};
			createInfo.sType = StructureType::eImageViewCreateInfo;
			createInfo.image = images[i];
			createInfo.viewType = ImageViewType::e2D;
			createInfo.format = swapchainCreateInfo.imageFormat;
			createInfo.components.r = ComponentSwizzle::eIdentity;
			createInfo.components.g = ComponentSwizzle::eIdentity;
			createInfo.components.b = ComponentSwizzle::eIdentity;
			createInfo.components.a = ComponentSwizzle::eIdentity;
			createInfo.subresourceRange.aspectMask = ImageAspectFlagBits::eColor;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;
			result = device.createImageView(&createInfo, nullptr, &views[i]);
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
		framebuffers.resize(views.size());
		for (size_t i = 0; i < views.size(); i++) {
			ImageView attachments[] = { views[i] };
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
		imageFences.resize(images.size(), VK_NULL_HANDLE);
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
	for (auto view : views) device.destroyImageView(view, nullptr);
	device.freeCommandBuffers(commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
	device.destroyPipeline(graphicsPipeline, nullptr);
	device.destroyPipelineLayout(pipelineLayout, nullptr);
	device.destroyRenderPass(renderPass, nullptr);
	device.destroyCommandPool(commandPool, nullptr);
	device.destroySwapchainKHR(swapchain, nullptr);
	device.destroy(nullptr);
	instance.destroySurfaceKHR(surface, nullptr);
#ifdef _DEBUG
	instance.destroyDebugUtilsMessengerEXT(debugger, nullptr);
#endif
	instance.destroy();
}
