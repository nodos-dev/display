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

// Returns an id for `monitor` that survives GLFW re-enumeration so it's
// safe to persist inside a port identifier or serialized pin value.
//   macOS: CGDirectDisplayID (raw GLFWmonitor* is invalidated whenever
//          monitor configuration changes — including during window
//          creation, causing glfwGetMonitorPos on a stale pointer to
//          crash in _glfwGetMonitorPosCocoa).
//   Windows / Linux: the raw pointer, which GLFW keeps stable there.
// Returns 0 if `monitor` is null.
uintptr_t GetMonitorStableId(GLFWmonitor* monitor);

// Inverse of GetMonitorStableId: walks the current `glfwGetMonitors()`
// list and returns the monitor whose stable id matches `id`, or nullptr.
// Callers must only use the returned pointer transiently — do not store
// it; re-resolve via this function when needed.
GLFWmonitor* GetMonitorByStableId(uintptr_t id);
}
