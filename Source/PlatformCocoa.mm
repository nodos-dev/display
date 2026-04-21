// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include "Platform.h"

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <Nodos/Plugin.hpp>

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

void RunOnMainThread(std::function<void()> fn)
{
	if (!fn)
		return;
	// If the engine exposes a main-thread dispatcher (plugin API >= 41.1),
	// route through it. Otherwise fall back to running inline: that will
	// crash on AppKit calls, but older engines have no way to honor the
	// requirement and the LogE makes the reason visible.
	if (nosEngine.RunOnMainThread)
	{
		nosEngine.RunOnMainThread(
			[](void* p) { (*static_cast<std::function<void()>*>(p))(); },
			&fn,
			NOS_TRUE);
		return;
	}
	static bool warned = false;
	if (!warned)
	{
		nosEngine.LogE("nos.display: host engine has no RunOnMainThread; AppKit calls will likely crash.");
		warned = true;
	}
	fn();
}
}
