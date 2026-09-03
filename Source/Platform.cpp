// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include "Platform.h"

#include <Nodos/Plugin.hpp>

namespace nos::display::platform
{
void RunOnMainThread(std::function<void()> fn)
{
	if (!fn)
		return;
	// `fn` is a by-value parameter and the call blocks, so passing its
	// address to the dispatcher is safe.
	if (nosEngine.RunOnMainThread &&
		nosEngine.RunOnMainThread([](void* p) { (*static_cast<std::function<void()>*>(p))(); }, &fn, NOS_TRUE) ==
			NOS_RESULT_SUCCESS)
		return;
	// Nothing to dispatch to: an engine without the service, or a host with
	// no launcher. Run inline and say why. AppKit calls will likely crash
	// from here on macOS, and on Windows GLFW stops noticing monitors being
	// plugged in or out.
	static bool warned = false;
	if (!warned)
	{
		nosEngine.LogE("nos.display: host engine has no main-thread dispatcher; window calls will run inline.");
		warned = true;
	}
	fn();
}
}
