// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include "Platform.h"

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace nos::display::platform
{
const char* GetAdapterName(GLFWmonitor* monitor)
{
	// macOS has no equivalent of the Windows GDI adapter name; fall back to
	// the GLFW monitor name.
	return glfwGetMonitorName(monitor);
}

void* GetVulkanWindowHandle(GLFWwindow* window)
{
	// nosSysVulkan's CreateWindowSurface expects a CAMetalLayer on macOS.
	// GLFW only attaches one lazily (inside glfwCreateWindowSurface), so
	// create & attach it to the window's content view ourselves.
	NSWindow* nsWindow = (NSWindow*)glfwGetCocoaWindow(window);
	if (!nsWindow)
		return nullptr;
	NSView* view = [nsWindow contentView];
	if (!view)
		return nullptr;

	CAMetalLayer* metalLayer = nil;
	if ([view.layer isKindOfClass:[CAMetalLayer class]])
	{
		metalLayer = (CAMetalLayer*)view.layer;
	}
	else
	{
		metalLayer = [CAMetalLayer layer];
		// MoltenVK/CAMetalLayer need a concrete pixel format — without one,
		// the layer never produces a drawable and the window stays blank.
		metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
		metalLayer.framebufferOnly = YES;

		CGFloat scale = [nsWindow backingScaleFactor];
		metalLayer.contentsScale = scale;
		metalLayer.frame = view.bounds;
		// Do not set drawableSize here. MoltenVK sets it to match the
		// swapchain's imageExtent when vkCreateSwapchainKHR runs; setting
		// it first would cause surfaceCapabilities.currentExtent to report
		// this value and reject a swapchain created at a different extent.
		metalLayer.needsDisplayOnBoundsChange = YES;

		// AppKit rule: the layer must be assigned before wantsLayer is set.
		// If wantsLayer is set first, AppKit installs a default CALayer and
		// our CAMetalLayer ends up as a sibling that never gets composited.
		[view setLayer:metalLayer];
		[view setWantsLayer:YES];
	}
	return (__bridge void*)metalLayer;
}

uintptr_t GetMonitorStableId(GLFWmonitor* monitor)
{
	if (!monitor)
		return 0;
	// CGDirectDisplayID is stable across GLFW re-enumerations and across
	// monitor connect/disconnect events, unlike the raw GLFWmonitor*.
	return static_cast<uintptr_t>(glfwGetCocoaMonitor(monitor));
}

GLFWmonitor* GetMonitorByStableId(uintptr_t id)
{
	if (!id)
		return nullptr;
	int count = 0;
	GLFWmonitor** monitors = glfwGetMonitors(&count);
	for (int i = 0; i < count; ++i)
	{
		if (GetMonitorStableId(monitors[i]) == id)
			return monitors[i];
	}
	return nullptr;
}
}
