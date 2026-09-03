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

uintptr_t GetMonitorStableId(GLFWmonitor* monitor)
{
	// GLFW keeps monitor pointers stable across enumerations on X11, so
	// the pointer itself is a fine persistent identifier here.
	return reinterpret_cast<uintptr_t>(monitor);
}

GLFWmonitor* GetMonitorByStableId(uintptr_t id)
{
	if (!id)
		return nullptr;
	int count = 0;
	GLFWmonitor** monitors = glfwGetMonitors(&count);
	for (int i = 0; i < count; ++i)
	{
		if (reinterpret_cast<uintptr_t>(monitors[i]) == id)
			return monitors[i];
	}
	return nullptr;
}
}
