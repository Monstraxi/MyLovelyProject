#include <iostream>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include "VulkanApp.h"

int main()
{
	VulkanApp App;

	App.Run();
}