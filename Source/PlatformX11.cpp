// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include "Platform.h"

#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace nos::display::platform
{
const char* GetAdapterName(GLFWmonitor* monitor)
{
	// X11 has no equivalent of the Windows GDI adapter name; fall back to the
	// GLFW monitor name so callers still get a stable string identifier.
	return glfwGetMonitorName(monitor);
}

void* GetVulkanWindowHandle(GLFWwindow* window)
{
	return reinterpret_cast<void*>(static_cast<uintptr_t>(glfwGetX11Window(window)));
}

void RunOnMainThread(std::function<void()> fn)
{
	if (fn)
		fn();
}
}
