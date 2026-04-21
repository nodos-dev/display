// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#pragma once

#include <functional>

struct GLFWmonitor;
struct GLFWwindow;

namespace nos::display::platform
{
// Runs `fn` on the launcher's main/UI thread and blocks until it returns.
// On macOS this uses nosEngine.RunOnMainThread because AppKit window and
// view APIs must be accessed from the main thread. On Windows and Linux
// it runs inline — GLFW's Win32/X11 backends tolerate off-main-thread use
// in the ways this plugin exercises, and the extra dispatch would add
// latency to the per-frame execute path.
void RunOnMainThread(std::function<void()> fn);

// Returns a platform-specific adapter name for a monitor.
// On Windows this is the GDI adapter name (e.g. "\\.\\DISPLAY1"), elsewhere
// the GLFW monitor name is returned as a best-effort identifier.
const char* GetAdapterName(GLFWmonitor* monitor);

// Returns the native window handle in the form expected by
// nosVulkan->CreateWindowSurface for this platform:
//   Windows: HWND
//   Linux/X11: Window (xcb/xlib window id)
//   macOS: CAMetalLayer* attached to the GLFW window's content view
void* GetVulkanWindowHandle(GLFWwindow* window);
}
