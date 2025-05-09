#pragma once

#define VK_CHECK(x) {VkResult result = x; if(result != VK_SUCCESS) { std::cerr << "Vulkan function call failed in " << __FILE__ << " at line " << __LINE__ << " with error " << result << std::endl; __debugbreak(); }}

#include <vector>
#include <array>
#include <optional>
#include <fstream>
#include <iostream>
#include <algorithm>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct SwapchainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

static std::vector<char> ReadFile(const std::string& filePath)
{
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);
	if (!file.is_open())
		std::cerr << "Failed to open file!" << std::endl;

	size_t length = (size_t)file.tellg();
	std::vector<char> data(length);

	file.seekg(0);
	file.read(data.data(), length);
	file.close();
	return data;
}

class VulkanApp
{
public:
	VulkanApp();
	~VulkanApp();
	void Run();
private:
	void InitWindow();
	void CreateSurface();
	void InitVulkan();
	void PickPhysicalDevice();
	void CreateLogicalDevice();
	void CreateSwapchain();
	void CreateRenderPass();
	void CreatePipeline();
	void CreateFramebuffers();
	void CreateCommandPool();
	void CreateCommandBuffer();
	void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void DrawFrame();
	void CreateSyncObjects();

	GLFWwindow* m_Window = nullptr;

	VkInstance m_Instance = nullptr;
	VkSurfaceKHR m_Surface = nullptr;
	VkPhysicalDevice m_PhysicalDevice = nullptr;
	VkDevice m_Device = nullptr;
	VkQueue m_GraphicsQueue = nullptr;
	VkQueue m_PresentQueue = nullptr;

	VkSwapchainKHR m_Swapchain = nullptr;
	std::vector<VkImage> m_SwapchainImages;
	VkFormat m_SwapchainFormat = VK_FORMAT_UNDEFINED;
	VkExtent2D m_SwapchainExtent = { 0, 0 };
	std::vector<VkImageView> m_SwapchainImageViews;
	std::vector<VkFramebuffer> m_SwapchainFramebuffers;

	VkPipeline m_GraphicsPipeline = nullptr;
	VkRenderPass m_RenderPass = nullptr;
	VkPipelineLayout m_PipelineLayout = nullptr;

	VkCommandPool m_CommandPool = nullptr;
	VkCommandBuffer m_CommandBuffer = nullptr;

	VkSemaphore m_ImageAvailableSemaphore = nullptr;
	VkSemaphore m_RenderFinishedSemaphore = nullptr;
	VkFence m_InFlightFence = nullptr;
private:
	#ifdef DEBUG 
	bool m_EnableValidationLayer = true;
	#else
	bool m_EnableValidationLayer = false;
	#endif

	std::array<const char*, 1> m_ValidationLayers = { "VK_LAYER_KHRONOS_validation" };
	std::array<const char*, 1> m_DeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	bool CheckValidationLayersSupport() const;
	bool CheckPhysicalDeviceExtensionsSupport(VkPhysicalDevice physicalDevice) const;
	bool IsPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice) const;
	std::optional<uint32_t> GetQueueFamily(VkPhysicalDevice physicalDevice) const;
	SwapchainSupportDetails QuerySwapchainSupportDetails(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) const;
	VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
	VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;
};