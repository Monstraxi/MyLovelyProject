#include "VulkanApp.h"

VulkanApp::VulkanApp() {}

VulkanApp::~VulkanApp()
{
	vkDeviceWaitIdle(m_Device);

	vkDestroyFence(m_Device, m_InFlightFence, nullptr);
	vkDestroySemaphore(m_Device, m_RenderFinishedSemaphore, nullptr);
	vkDestroySemaphore(m_Device, m_ImageAvailableSemaphore, nullptr);
	vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
	for (VkFramebuffer framebuffer : m_SwapchainFramebuffers)
		vkDestroyFramebuffer(m_Device, framebuffer, nullptr);
	vkDestroyPipeline(m_Device, m_GraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
	vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
	for (VkImageView view : m_SwapchainImageViews)
		vkDestroyImageView(m_Device, view, nullptr);
	vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
	vkDestroyDevice(m_Device, nullptr);
	vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
	vkDestroyInstance(m_Instance, nullptr);
	glfwDestroyWindow(m_Window);
	glfwTerminate();
}

void VulkanApp::Run()
{
	InitWindow();
	InitVulkan();

	while (!glfwWindowShouldClose(m_Window))
	{
		glfwPollEvents();
		DrawFrame();
	}
}

void VulkanApp::InitWindow()
{
	if (!glfwInit())
		std::cerr << "Failed to initialize GLFW!" << std::endl;

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	m_Window = glfwCreateWindow(1280, 720, "VulkanApp", NULL, NULL);

	if (!m_Window)
		std::cerr << "Failed to create GLFW window!" << std::endl;
}

void VulkanApp::InitVulkan()
{
	if (m_EnableValidationLayer && !CheckValidationLayersSupport())
		std::cerr << "Vulkan validation layer not supported!" << std::endl;

	uint32_t extensionCount = 0;
	const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCount);

	VkApplicationInfo appInfo{};
	appInfo.apiVersion = VK_API_VERSION_1_0;
	appInfo.pApplicationName = "VulkanApp";
	appInfo.applicationVersion = VK_MAKE_API_VERSION(1, 1, 0, 0);
	appInfo.pEngineName = "NoEngine";
	appInfo.engineVersion = VK_MAKE_API_VERSION(1, 1, 0, 0);

	VkInstanceCreateInfo instanceInfo{};
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pApplicationInfo = &appInfo;
	instanceInfo.enabledExtensionCount = extensionCount;
	instanceInfo.ppEnabledExtensionNames = extensions;
	if (m_EnableValidationLayer)
	{
		instanceInfo.enabledLayerCount = (uint32_t)m_ValidationLayers.size();
		instanceInfo.ppEnabledLayerNames = m_ValidationLayers.data();
	}
	else
	{
		instanceInfo.enabledLayerCount = 0;
		instanceInfo.ppEnabledLayerNames = nullptr;
	}

	VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &m_Instance));

	CreateSurface();
	PickPhysicalDevice();
	CreateLogicalDevice();
	CreateSwapchain();
	CreateRenderPass();
	CreatePipeline();
	CreateFramebuffers();
	CreateCommandPool();
	CreateCommandBuffer();
	CreateSyncObjects();
}

void VulkanApp::DrawFrame()
{
	vkWaitForFences(m_Device, 1, &m_InFlightFence, VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(m_Device, 1, &m_InFlightFence);

	uint32_t imageIndex = 0;
	vkAcquireNextImageKHR(m_Device, m_Swapchain, std::numeric_limits<uint64_t>::max(), m_ImageAvailableSemaphore, nullptr, &imageIndex);
	vkResetCommandBuffer(m_CommandBuffer, 0);
	RecordCommandBuffer(m_CommandBuffer, imageIndex);

	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_CommandBuffer;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &m_ImageAvailableSemaphore;
	submitInfo.pWaitDstStageMask = &waitStage;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &m_RenderFinishedSemaphore;
	
	VK_CHECK(vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_InFlightFence));

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_Swapchain;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_RenderFinishedSemaphore;
	presentInfo.pResults = nullptr;

	VK_CHECK(vkQueuePresentKHR(m_PresentQueue, &presentInfo));
}

void VulkanApp::CreateSurface()
{
	VK_CHECK(glfwCreateWindowSurface(m_Instance, m_Window, nullptr, &m_Surface));
}

void VulkanApp::PickPhysicalDevice()
{
	uint32_t physicalDeviceCount = 0;
	vkEnumeratePhysicalDevices(m_Instance, &physicalDeviceCount, nullptr);
	std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
	vkEnumeratePhysicalDevices(m_Instance, &physicalDeviceCount, physicalDevices.data());

	for (VkPhysicalDevice physicalDevice : physicalDevices)
	{
		if (IsPhysicalDeviceSuitable(physicalDevice))
		{
			m_PhysicalDevice = physicalDevice;
			break;
		}
	}
	if (!m_PhysicalDevice)
		std::cerr << "No suitable physical device found!" << std::endl;
}

void VulkanApp::CreateLogicalDevice()
{
	std::optional<uint32_t> queueFamIndex = GetQueueFamily(m_PhysicalDevice);
	std::array<float, 1> queuePriorities = { 1.0f };
	std::array<VkDeviceQueueCreateInfo, 2> queueInfos{};
	queueInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfos[0].queueFamilyIndex = queueFamIndex.value();
	queueInfos[0].queueCount = 1;
	queueInfos[0].pQueuePriorities = queuePriorities.data();

	VkPhysicalDeviceFeatures deviceFeatures{};

	VkDeviceCreateInfo deviceInfo{};
	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.queueCreateInfoCount = 1;
	deviceInfo.pQueueCreateInfos = queueInfos.data();
	deviceInfo.pEnabledFeatures = &deviceFeatures;
	deviceInfo.enabledExtensionCount = (uint32_t)m_DeviceExtensions.size();
	deviceInfo.ppEnabledExtensionNames = m_DeviceExtensions.data();
	if (m_EnableValidationLayer)
	{
		deviceInfo.enabledLayerCount = (uint32_t)m_ValidationLayers.size();
		deviceInfo.ppEnabledLayerNames = m_ValidationLayers.data();
	}
	else
	{
		deviceInfo.enabledLayerCount = 0;
		deviceInfo.ppEnabledLayerNames = nullptr;
	}

	VK_CHECK(vkCreateDevice(m_PhysicalDevice, &deviceInfo, nullptr, &m_Device));

	vkGetDeviceQueue(m_Device, queueFamIndex.value(), 0, &m_GraphicsQueue);
	vkGetDeviceQueue(m_Device, queueFamIndex.value(), 0, &m_PresentQueue);
}

void VulkanApp::CreateSwapchain()
{
	SwapchainSupportDetails details = QuerySwapchainSupportDetails(m_PhysicalDevice, m_Surface);
	VkSurfaceFormatKHR format = ChooseSurfaceFormat(details.formats);
	VkPresentModeKHR presentMode = ChoosePresentMode(details.presentModes);
	VkExtent2D extent = ChooseSwapExtent(details.capabilities);
	uint32_t imageCount = details.capabilities.minImageCount + 1;
	if (details.capabilities.maxImageCount > 0 && imageCount > details.capabilities.maxImageCount)
		imageCount = details.capabilities.maxImageCount;

	uint32_t queueFamilyIndex = GetQueueFamily(m_PhysicalDevice).value();

	VkSwapchainCreateInfoKHR swapchainInfo{};
	swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainInfo.flags = 0;
	swapchainInfo.surface = m_Surface;

	swapchainInfo.minImageCount = imageCount;
	swapchainInfo.imageFormat = format.format;
	swapchainInfo.imageColorSpace = format.colorSpace;
	swapchainInfo.presentMode = presentMode;
	swapchainInfo.imageExtent = extent;
	swapchainInfo.imageArrayLayers = 1;

	swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainInfo.preTransform = details.capabilities.currentTransform;
	swapchainInfo.clipped = VK_TRUE;

	swapchainInfo.oldSwapchain = nullptr;

	VK_CHECK(vkCreateSwapchainKHR(m_Device, &swapchainInfo, nullptr, &m_Swapchain));

	vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, nullptr);
	m_SwapchainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, m_SwapchainImages.data());
	m_SwapchainFormat = format.format;
	m_SwapchainExtent = extent;

	m_SwapchainImageViews.resize(imageCount);
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.flags = 0;

	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format.format;
	viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	for (uint32_t i = 0; i < imageCount; i++)
	{
		viewInfo.image = m_SwapchainImages[i];
		VK_CHECK(vkCreateImageView(m_Device, &viewInfo, nullptr, &m_SwapchainImageViews[i]));
	}
}

void VulkanApp::CreateRenderPass()
{
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = m_SwapchainFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachmentRef.attachment = 0;

	VkSubpassDescription subpassDesc{};
	subpassDesc.flags = 0;
	subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDesc.colorAttachmentCount = 1;
	subpassDesc.pColorAttachments = &colorAttachmentRef;

	VkSubpassDependency subpassDep{};
	subpassDep.dependencyFlags = 0;
	subpassDep.srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDep.dstSubpass = 0;
	subpassDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDep.srcAccessMask = 0;
	subpassDep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDesc;
	renderPassInfo.pDependencies = &subpassDep;

	VK_CHECK(vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_RenderPass));
}

void VulkanApp::CreatePipeline()
{
	std::vector<char> vertShaderCode = ReadFile("res/shaders/vert.spv");
	std::vector<char> fragShaderCode = ReadFile("res/shaders/frag.spv");

	VkShaderModule vertShaderModule = nullptr;
	VkShaderModule fragShaderModule = nullptr;

	VkShaderModuleCreateInfo shaderModuleInfo{};
	shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleInfo.flags = 0;

	shaderModuleInfo.codeSize = vertShaderCode.size();
	shaderModuleInfo.pCode = (uint32_t*)vertShaderCode.data();
	VK_CHECK(vkCreateShaderModule(m_Device, &shaderModuleInfo, nullptr, &vertShaderModule));

	shaderModuleInfo.codeSize = fragShaderCode.size();
	shaderModuleInfo.pCode = (uint32_t*)fragShaderCode.data();
	VK_CHECK(vkCreateShaderModule(m_Device, &shaderModuleInfo, nullptr, &fragShaderModule));

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageInfos{};

	{
		VkPipelineShaderStageCreateInfo vertStageInfo{};
		vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertStageInfo.flags = 0;
		vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertStageInfo.pName = "main";
		vertStageInfo.module = vertShaderModule;

		VkPipelineShaderStageCreateInfo fragStageInfo{};
		fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragStageInfo.flags = 0;
		fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragStageInfo.pName = "main";
		fragStageInfo.module = fragShaderModule;

		shaderStageInfos[0] = vertStageInfo;
		shaderStageInfos[1] = fragStageInfo;
	}

	std::array<VkDynamicState, 2> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
	dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateInfo.flags = 0;
	dynamicStateInfo.dynamicStateCount = (uint32_t)dynamicStates.size();
	dynamicStateInfo.pDynamicStates = dynamicStates.data();

	VkPipelineVertexInputStateCreateInfo vertexInputStateInfo{};
	vertexInputStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateInfo.flags = 0;
	vertexInputStateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputStateInfo.pVertexAttributeDescriptions = nullptr;
	vertexInputStateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateInfo.pVertexBindingDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo{};
	inputAssemblyStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateInfo.flags = 0;
	inputAssemblyStateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportStateInfo{};
	viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateInfo.viewportCount = 1;
	viewportStateInfo.scissorCount = 1;
	
	VkPipelineRasterizationStateCreateInfo rasterizationStateInfo{};
	rasterizationStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateInfo.flags = 0;
	rasterizationStateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationStateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizationStateInfo.lineWidth = 1.0f;
	rasterizationStateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateInfo.depthBiasEnable = VK_FALSE;
	rasterizationStateInfo.depthBiasClamp = 0.0f;
	rasterizationStateInfo.depthBiasSlopeFactor = 0.0f;
	rasterizationStateInfo.depthBiasConstantFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisampleStateInfo{};
	multisampleStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateInfo.flags = 0;
	multisampleStateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateInfo.minSampleShading = 1.0f;
	multisampleStateInfo.pSampleMask = nullptr;
	multisampleStateInfo.alphaToCoverageEnable = VK_FALSE;
	multisampleStateInfo.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState blendAttachmentState{};
	blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD; 

	VkPipelineColorBlendStateCreateInfo blendStateInfo{};
	blendStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blendStateInfo.flags = 0;
	blendStateInfo.attachmentCount = 1;
	blendStateInfo.pAttachments = &blendAttachmentState;
	blendStateInfo.logicOpEnable = VK_FALSE;
	blendStateInfo.logicOp = VK_LOGIC_OP_COPY;
	blendStateInfo.blendConstants[0] = 0.0f;
	blendStateInfo.blendConstants[1] = 0.0f;
	blendStateInfo.blendConstants[2] = 0.0f;
	blendStateInfo.blendConstants[3] = 0.0f;

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.flags = 0;
	layoutInfo.setLayoutCount = 0;
	layoutInfo.pSetLayouts = nullptr;
	layoutInfo.pushConstantRangeCount = 0;
	layoutInfo.pPushConstantRanges = nullptr;

	VK_CHECK(vkCreatePipelineLayout(m_Device, &layoutInfo, nullptr, &m_PipelineLayout));

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.flags = 0;
	pipelineInfo.basePipelineIndex = -1;
	pipelineInfo.basePipelineHandle = nullptr;
	pipelineInfo.layout = m_PipelineLayout;

	pipelineInfo.pVertexInputState = &vertexInputStateInfo;
	pipelineInfo.pInputAssemblyState = &inputAssemblyStateInfo;
	pipelineInfo.stageCount = (uint32_t)shaderStageInfos.size();
	pipelineInfo.pStages = shaderStageInfos.data();
	pipelineInfo.pTessellationState = nullptr;
	pipelineInfo.pViewportState = &viewportStateInfo;
	pipelineInfo.pRasterizationState = &rasterizationStateInfo;
	pipelineInfo.pMultisampleState = &multisampleStateInfo;
	pipelineInfo.pDepthStencilState = nullptr;
	pipelineInfo.pColorBlendState = &blendStateInfo;
	pipelineInfo.pDynamicState = &dynamicStateInfo;

	pipelineInfo.renderPass = m_RenderPass;
	pipelineInfo.subpass = 0;

	VK_CHECK(vkCreateGraphicsPipelines(m_Device, nullptr, 1, &pipelineInfo, nullptr, &m_GraphicsPipeline));

	vkDestroyShaderModule(m_Device, vertShaderModule, nullptr);
	vkDestroyShaderModule(m_Device, fragShaderModule, nullptr);
}

void VulkanApp::CreateFramebuffers()
{
	m_SwapchainFramebuffers.resize(m_SwapchainImageViews.size());
	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.flags = 0;
	framebufferInfo.layers = 1;
	framebufferInfo.renderPass = m_RenderPass;
	framebufferInfo.width = m_SwapchainExtent.width;
	framebufferInfo.height = m_SwapchainExtent.height;

	for (size_t i = 0; i < m_SwapchainImageViews.size(); i++)
	{
		std::array<VkImageView, 1> attachments = {
			m_SwapchainImageViews[i]
		};
		framebufferInfo.attachmentCount = (uint32_t)attachments.size();
		framebufferInfo.pAttachments = attachments.data();

		VK_CHECK(vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr, &m_SwapchainFramebuffers[i]));
	}
}

void VulkanApp::CreateCommandPool()
{
	VkCommandPoolCreateInfo commandPoolInfo{};
	commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolInfo.queueFamilyIndex = GetQueueFamily(m_PhysicalDevice).value();

	VK_CHECK(vkCreateCommandPool(m_Device, &commandPoolInfo, nullptr, &m_CommandPool));
}

void VulkanApp::CreateCommandBuffer()
{
	VkCommandBufferAllocateInfo commandBufferInfo{};
	commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferInfo.commandPool = m_CommandPool;
	commandBufferInfo.commandBufferCount = 1;

	VK_CHECK(vkAllocateCommandBuffers(m_Device, &commandBufferInfo, &m_CommandBuffer));
}

void VulkanApp::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	VkCommandBufferBeginInfo commandBeginInfo{};
	commandBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBeginInfo.flags = 0;
	commandBeginInfo.pInheritanceInfo = nullptr;

	VK_CHECK(vkBeginCommandBuffer(commandBuffer, &commandBeginInfo));

	VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };

	VkRenderPassBeginInfo renderPassBeginInfo{};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = m_RenderPass;
	renderPassBeginInfo.framebuffer = m_SwapchainFramebuffers[imageIndex];
	renderPassBeginInfo.renderArea.offset = { 0, 0 };
	renderPassBeginInfo.renderArea.extent = m_SwapchainExtent;
	renderPassBeginInfo.clearValueCount = 1;
	renderPassBeginInfo.pClearValues = &clearColor;

	vkCmdBeginRenderPass(m_CommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);

	VkViewport viewport{};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = (float)m_SwapchainExtent.width;
	viewport.height = (float)m_SwapchainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	vkCmdSetViewport(m_CommandBuffer, 0, 1, &viewport);
	
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = m_SwapchainExtent;

	vkCmdSetScissor(m_CommandBuffer, 0, 1, &scissor);

	vkCmdDraw(m_CommandBuffer, 6, 1, 0, 0);

	vkCmdEndRenderPass(m_CommandBuffer);

	VK_CHECK(vkEndCommandBuffer(m_CommandBuffer));
}

void VulkanApp::CreateSyncObjects()
{
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreInfo.flags = 0;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphore));
	VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphore));
	VK_CHECK(vkCreateFence(m_Device, &fenceInfo, nullptr, &m_InFlightFence));
}

bool VulkanApp::IsPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice) const
{
	bool hasQueueFamily = GetQueueFamily(physicalDevice).has_value();
	bool deviceExtensionsSupported = CheckPhysicalDeviceExtensionsSupport(physicalDevice);

	SwapchainSupportDetails details = QuerySwapchainSupportDetails(physicalDevice, m_Surface);
	bool swapchainAdequate = !details.formats.empty() && !details.presentModes.empty();

	return hasQueueFamily && deviceExtensionsSupported && swapchainAdequate;
}

std::optional<uint32_t> VulkanApp::GetQueueFamily(VkPhysicalDevice physicalDevice) const
{
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());
	for (uint32_t i = 0; i < queueFamilyCount; i++)
	{
		uint32_t presentSupport = 0;
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, m_Surface, &presentSupport);
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && presentSupport)
			return std::optional<uint32_t>(i);
	}
	return std::optional<uint32_t>();
}

bool VulkanApp::CheckValidationLayersSupport() const
{
	uint32_t propertyCount = 0;
	vkEnumerateInstanceLayerProperties(&propertyCount, nullptr);
	std::vector<VkLayerProperties> properties(propertyCount);
	vkEnumerateInstanceLayerProperties(&propertyCount, properties.data());

	for (const char* layer : m_ValidationLayers)
	{
		bool layerFound = false;
		for (const VkLayerProperties& property : properties)
		{
			if (strcmp(property.layerName, layer))
			{
				layerFound = true;
				break;
			}
		}
		if (!layerFound)
			return false;
	}

	return true;
}

bool VulkanApp::CheckPhysicalDeviceExtensionsSupport(VkPhysicalDevice physicalDevice) const
{
	uint32_t propertyCount = 0;
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &propertyCount, nullptr);
	std::vector<VkExtensionProperties> properties(propertyCount);
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &propertyCount, properties.data());

	for (const char* extension : m_DeviceExtensions)
	{
		bool extensionFound = false;
		for (const VkExtensionProperties& property : properties)
		{
			if (strcmp(property.extensionName, extension))
			{
				extensionFound = true;
				break;
			}
		}
		if (!extensionFound)
			return false;
	}

	return true;
}

SwapchainSupportDetails VulkanApp::QuerySwapchainSupportDetails(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) const
{
	SwapchainSupportDetails details{};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities);

	{
		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
		if (formatCount != 0)
		{
			details.formats.resize((size_t)formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats.data());
		}
	}

	{
		uint32_t presentModeCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
		if (presentModeCount != 0)
		{
			details.presentModes.resize((size_t)presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, details.presentModes.data());
		}
	}

	return details;
}

VkSurfaceFormatKHR VulkanApp::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const
{
	for (const VkSurfaceFormatKHR& format : availableFormats)
	{
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return format;
	}

	return availableFormats[0];
}

VkPresentModeKHR VulkanApp::ChoosePresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const
{
	for (const VkPresentModeKHR& presentMode : availablePresentModes)
	{
		if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			return presentMode;
	}

	return availablePresentModes[0];
}

VkExtent2D VulkanApp::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		return capabilities.currentExtent;
	
	VkExtent2D extent{};
	int width = 0, height = 0;
	glfwGetFramebufferSize(m_Window, &width, &height);
	extent.width = std::clamp(uint32_t(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	extent.height = std::clamp(uint32_t(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

	return extent;
}