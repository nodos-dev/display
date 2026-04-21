// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include "Platform.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace nos::display::platform
{
const char* GetAdapterName(GLFWmonitor* monitor)
{
	return glfwGetWin32Adapter(monitor);
}

void* GetVulkanWindowHandle(GLFWwindow* window)
{
	return reinterpret_cast<void*>(glfwGetWin32Window(window));
}

void RunOnMainThread(std::function<void()> fn)
{
	if (fn)
		fn();
}
}
