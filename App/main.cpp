#include <iostream>

#define GLFW_INCLUDE_VULKAN
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

int main()
{
	GLFWwindow* window = nullptr;
	if (!glfwInit)
		return -1;

	window = glfwCreateWindow(1280, 720, "App", NULL, NULL);

	if (!window)
	{
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}

	glfwTerminate();
}