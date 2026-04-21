#include "CustomResolutionBase.h"
#include "Platform.h"

#include <Nodos/Plugin.hpp>
#include <nosSysVulkan/Helpers.hpp>

#include <Nodos/Utils/Stopwatch.hpp>
#include <unordered_map>
#include "GLFW/glfw3.h"


namespace nos::display
{
NOS_REGISTER_NAME(Internal_CustomResolutionRequested)
std::vector<std::string> GetPossibleAdapterNames()
{
	std::vector<std::string> adapterNames;
	int monitorCount;
	GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
	for (int i = 0; i < monitorCount; i++)
		adapterNames.push_back(platform::GetAdapterName(monitors[i]));
	return adapterNames;
}

GLFWmonitor* GetGLFWMonitorFromAdapterName(const char* adapterName)
{
	int monitorCount;
	GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
	for (int i = 0; i < monitorCount; i++)
	{
		const char* name = platform::GetAdapterName(monitors[i]);
		if (strcmp(name, adapterName) == 0)
			return monitors[i];
	}
	return nullptr;
}

GLFWmonitor* get_current_monitor(GLFWwindow* window)
{
	int nmonitors, i;
	int wx, wy, ww, wh;
	int mx, my, mw, mh;
	int overlap, bestoverlap;
	GLFWmonitor* bestmonitor;
	GLFWmonitor** monitors;
	const GLFWvidmode* mode;
	
	bestoverlap = 0;
	bestmonitor = NULL;

	glfwGetWindowPos(window, &wx, &wy);
	glfwGetWindowSize(window, &ww, &wh);
	monitors = glfwGetMonitors(&nmonitors);

	for (i = 0; i < nmonitors; i++) {
		mode = glfwGetVideoMode(monitors[i]);
		glfwGetMonitorPos(monitors[i], &mx, &my);
		mw = mode->width;
		mh = mode->height;

		overlap =
			std::max(0, std::min(wx + ww, mx + mw) - std::max(wx, mx)) *
			std::max(0, std::min(wy + wh, my + mh) - std::max(wy, my));

		if (bestoverlap < overlap) {
			bestoverlap = overlap;
			bestmonitor = monitors[i];
		}
	}

	return bestmonitor;
}

GLFWmonitor* GetMonitorFromName(const char* monitorName)
{
	int monitorCount;
	GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
	for (int i = 0; i < monitorCount; i++)
	{
		const char* name = platform::GetAdapterName(monitors[i]);
		if (strcmp(name, monitorName) == 0)
			return monitors[i];
	}
	return nullptr;
}

NOS_REGISTER_NAME(Monitor)

struct DisplayOutNode : NodeContext
{
	nosResult OnCreate(const fb::Node* node) override
	{
		fb::TVisualizer visualizer;
		visualizer.type = fb::VisualizerType::COMBO_BOX;
		visualizer.name = std::string("Monitor_") + std::string(NodeId);
		SetPinVisualizer(NSN_Monitor, visualizer);
		UpdateStringList(std::string("Monitor_") + std::string(NodeId), {"NONE"});
		AddPinValueWatcher(NSN_Internal_CustomResolutionRequested);
		return NOS_RESULT_SUCCESS;
	}

	~DisplayOutNode()
	{
		platform::RunOnMainThread([&] { Clear(); });
	}

	bool CreateSwapchain()
	{
		nosSwapchainCreateInfo createInfo = {};
		createInfo.SurfaceHandle = Surface;
		createInfo.ImageFormat = ColorFormat;
		// Use the framebuffer (pixel) size rather than window (point) size —
		// on HiDPI/Retina these differ by the backing scale factor, and the
		// swapchain images must match the CAMetalLayer's drawableSize.
		int width = 0, height = 0;
		platform::RunOnMainThread([&] { glfwGetFramebufferSize(Window, &width, &height); });
		createInfo.Extent = { uint32_t(width), uint32_t(height) };
		createInfo.PresentMode = VSync ? NOS_PRESENT_MODE_FIFO : NOS_PRESENT_MODE_IMMEDIATE;
		nosResult res = nosVulkan->CreateSwapchain(&createInfo, &Swapchain.GetStorage(), &FrameCount);
		if (res != NOS_RESULT_SUCCESS)
		{
			nosEngine.LogE("DisplayOut: CreateSwapchain failed (extent %dx%d).", width, height);
			return false;
		}
		nosEngine.LogI("DisplayOut: swapchain created %dx%d, frames=%u.", width, height, FrameCount);
		nosSemaphoreCreateInfo semaphoreCreateInfo = {};
		semaphoreCreateInfo.Type = NOS_SEMAPHORE_TYPE_BINARY;
		Images.resize(FrameCount);
		if (FrameCount > 0)
			nosVulkan->GetSwapchainImages(Swapchain, &Images[0].GetStorage());
		WaitSemaphore.resize(FrameCount);
		SignalSemaphore.resize(FrameCount);
		WaitEvents.resize(FrameCount);
		for (int i = 0; i < FrameCount; i++)
		{
#ifdef CreateSemaphore
#undef CreateSemaphore
#endif
			nosVulkan->CreateSemaphore(&semaphoreCreateInfo, &WaitSemaphore[i].GetStorage());
			nosVulkan->CreateSemaphore(&semaphoreCreateInfo, &SignalSemaphore[i].GetStorage());
		}
		return true;
	}

	void Clear()
	{
		if(CustomResolutionActive)
			RevertMonitorResolution();
		DestroySwapchain();
		DestroyWindowSurface();
		DestroyWindow();
	}

	bool TryCreateSwapchain()
	{
		if (Swapchain)
			DestroySwapchain();
		if (!Surface)
			return false;
		if (!CreateSwapchain())
		{
			// Do not tear down the window/surface here. A swapchain create
			// can fail transiently (e.g. during a macOS resize the Metal
			// layer's drawable size and Vulkan surface capabilities can be
			// out of sync for one frame). Leaving Window/Surface intact
			// lets the next ExecuteNode retry via this same function
			// instead of permanently killing the node.
			return false;
		}
		return true;
	}

	void DestroySwapchain()
	{
		if (!Swapchain)
			return;
		nosCmd cmd;
		nosCmdBeginParams beginParams = { .Name = NOS_NAME("Window node flush cmd"), .AssociatedNodeId = NodeId, .OutCmdHandle = &cmd };
		nosVulkan->Begin(&beginParams);
		nosGPUEvent wait;
		nosCmdEndParams endParams = { .ForceSubmit = true, .OutGPUEventHandle = &wait };
		nosVulkan->End(cmd, &endParams);
		nosVulkan->WaitGpuEvent(&wait, UINT64_MAX);
		WaitSemaphore.clear();
		SignalSemaphore.clear();
		WaitEvents.clear();
		Images.clear();
		Swapchain = {};
	}

	void DestroyWindowSurface()
	{
		Surface = {};
	}

	void DestroyWindow()
	{
		ResetWindowCurrentMonitorCache();
		if (!Window)
			return;
		glfwDestroyWindow(Window);
		PortToGLFWMonitor.clear();
		glfwTerminate();
		Window = nullptr;
	}

	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		if (!Window)
		{
			static bool loggedNoWindow = false;
			if (!loggedNoWindow)
			{
				nosEngine.LogW("DisplayOut: no Window yet (OnEnterRunnerThread hasn't fired?)");
				loggedNoWindow = true;
			}
			return NOS_RESULT_FAILED;
		}
		nosScheduleNodeParams scheduleParams = {};
		scheduleParams.NodeId = NodeId;
		scheduleParams.Reset = false;
		scheduleParams.AddScheduleCount = 1;

		auto input = params.GetPinObject<sys::vulkan::Texture>(NOS_NAME("Input"));
		if (!input.IsValid())
		{
			static bool loggedNoInput = false;
			if (!loggedNoInput)
			{
				nosEngine.LogW("DisplayOut: Input pin is not connected; window will stay blank until a texture is connected.");
				loggedNoInput = true;
			}
			return NOS_RESULT_FAILED;
		}

		// glfwWindowShouldClose + glfwPollEvents must be on the main thread
		// (AppKit requirement on macOS; GLFW contract elsewhere). Batch them
		// into one dispatch to amortize the round-trip.
		bool shouldClose = false;
		platform::RunOnMainThread([&] {
			shouldClose = glfwWindowShouldClose(Window);
			if (!shouldClose)
				glfwPollEvents();
		});

		if (!shouldClose)
		{
			const char* errDesc;
			int err = glfwGetError(&errDesc);
			if (err != GLFW_NO_ERROR)
			{
				nosEngine.LogE("DisplayOut: GLFW error %d: %s", err, errDesc ? errDesc : "");
				return NOS_RESULT_FAILED;
			}

			// If an earlier TryCreateSwapchain failed, our per-frame arrays
			// are empty; attempt a fresh create before touching them so we
			// don't index out of bounds.
			if (!Swapchain || WaitSemaphore.empty())
			{
				if (!TryCreateSwapchain())
				{
					nosEngine.ScheduleNode(&scheduleParams);
					return NOS_RESULT_FAILED;
				}
				CurrentFrame = 0;
			}

			uint32_t imageIndex;
			constexpr uint32_t retryCount = 2;
			bool acquireOk = false;
			for (uint32_t retryIndex = 0; retryIndex < retryCount; retryIndex++)
			{
				auto acquireResult = nosVulkan->SwapchainAcquireNextImage(
					Swapchain, 100'000'000, &imageIndex, WaitSemaphore[CurrentFrame]);
				if (acquireResult == NOS_RESULT_SUCCESS)
				{
					acquireOk = true;
					break;
				}
				if (acquireResult == NOS_RESULT_TIMEOUT)
				{
					static bool loggedTimeout = false;
					if (!loggedTimeout)
					{
						nosEngine.LogW("DisplayOut: SwapchainAcquireNextImage timed out.");
						loggedTimeout = true;
					}
					return NOS_RESULT_PENDING;
				}
				if (retryIndex + 1 >= retryCount)
				{
					nosEngine.LogE("DisplayOut: SwapchainAcquireNextImage failed (result=%d) after retries.", acquireResult);
					break;
				}
				if (!TryCreateSwapchain())
				{
					nosEngine.LogW("DisplayOut: Swapchain recreate failed after acquire error (result=%d); will retry next frame.", acquireResult);
					break;
				}
			}
			if (!acquireOk)
			{
				// Keep the node scheduled so we try again next frame instead
				// of going permanently silent.
				nosEngine.ScheduleNode(&scheduleParams);
				return NOS_RESULT_FAILED;
			}
			if (WaitEvents[CurrentFrame])
			{
				nosVulkan->WaitGpuEvent(&WaitEvents[CurrentFrame], UINT64_MAX);
			}
			nosCmd cmd = sys::vulkan::BeginCmd(NOS_NAME("Window"), NodeId);
			nosVulkan->Copy(cmd, input, Images[imageIndex], 0);

			nosVulkan->ImageStateToPresent(cmd, Images[imageIndex]);
			nosVulkan->AddWaitSemaphoreToCmd(cmd, WaitSemaphore[CurrentFrame], 1);
			nosVulkan->AddSignalSemaphoreToCmd(cmd, SignalSemaphore[CurrentFrame], 1);
			nosCmdEndParams endParams{.ForceSubmit = true, .OutGPUEventHandle = &WaitEvents[CurrentFrame]};
			nosVulkan->End(cmd, &endParams);
			if (nosVulkan->SwapchainPresent(Swapchain, imageIndex, SignalSemaphore[CurrentFrame]) != NOS_RESULT_SUCCESS)
			{
				TryCreateSwapchain();
			}
			nosEngine.ScheduleNode(&scheduleParams);
			CurrentFrame = (CurrentFrame + 1) % FrameCount;
			if (!CustomResolutionActive)
			{
				bool refreshChanged = false;
				platform::RunOnMainThread([&] {
					if (auto monitor = GetGLFWMonitor())
					{
						auto mode = glfwGetVideoMode(monitor);
						if (mode && LastEffectiveRefreshRate != mode->refreshRate)
						{
							LastEffectiveRefreshRate = mode->refreshRate;
							refreshChanged = true;
						}
					}
				});
				if (refreshChanged)
					nosEngine.SendPathRestart(NodeId);
			}
		}
		else
		{
			platform::RunOnMainThread([&] { Clear(); });
			return NOS_RESULT_FAILED;
		}

		return NOS_RESULT_SUCCESS;
	}

	void OnExitRunnerThread(nosExitRunnerThreadParams const& params) override
	{
		if (!params.RunnerId)
			return;
		platform::RunOnMainThread([&] { Clear(); });
	}

	void OnEnterRunnerThread(nosEnterRunnerThreadParams const& params) override
	{
		if (!params.RunnerId)
			return;
		platform::RunOnMainThread([&] { OnEnterRunnerThreadMain(); });
	}

	void OnEnterRunnerThreadMain()
	{
		auto possibleMonitors = GetPossibleMonitors();
		UpdateStringList(std::string("Monitor_") + std::string(NodeId), possibleMonitors);
		// Auto-select the first real monitor when the user hasn't picked
		// one yet. possibleMonitors[0] is always the synthetic "NONE"
		// entry, so the first actionable monitor is at index 1 if present.
		if (!LockedMonitorPort && possibleMonitors.size() > 1)
		{
			const std::string& firstMonitor = possibleMonitors[1];
			LockedMonitorPort = GetPortFromString(firstMonitor.c_str());
			SetPinValue(NSN_Monitor, firstMonitor.c_str());
		}
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		Window = glfwCreateWindow(Resolution.x, Resolution.y, GetWindowName().c_str(), nullptr, nullptr);
		glfwSetInputMode(Window, GLFW_CURSOR, ShowCursor ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
		glfwSetWindowUserPointer(Window, this);
		glfwSetWindowSizeCallback(Window, [](GLFWwindow* window, int width, int height) {
			auto node = (DisplayOutNode*)glfwGetWindowUserPointer(window);
			node->ResetWindowCurrentMonitorCache();
			if (node->IsWindowLocked())
			{
				if (node->Resolution.x != width || node->Resolution.y != height)
				{
					if (auto monitor = node->GetGLFWMonitor())
					{
						int monitorPosX, monitorPosY;
						glfwGetMonitorPos(monitor, &monitorPosX, &monitorPosY);
						glfwSetWindowPos(window, monitorPosX, monitorPosY);
						glfwSetWindowSize(window, node->Resolution.x, node->Resolution.y);
						return;
					}
					else // Monitor lost?
						node->RevertMonitorResolution();
				}
				else
				{
					node->TryCreateSwapchain();
					return;
				}
			}
			node->TryCreateSwapchain();
		});
		RequeryPortToGLFWMonitors();
		for (auto& [port, monitor] : PortToGLFWMonitor)
		{
			glfwSetMonitorUserPointer(monitor, this);
			glfwSetMonitorCallback([](GLFWmonitor* monitor, int event) {
				auto node = (DisplayOutNode*)glfwGetMonitorUserPointer(monitor);
				node->RequeryPortToGLFWMonitors();
				node->ResetWindowCurrentMonitorCache();
			});
		}
		glfwSetWindowIconifyCallback(Window, [](GLFWwindow* window, int iconified) {
			auto node = (DisplayOutNode*)glfwGetWindowUserPointer(window);
			if (iconified == GLFW_TRUE && node->IsWindowLocked())
				glfwRestoreWindow(window);
			node->ResetWindowCurrentMonitorCache();
		});
		//glfwSetWindowFocusCallback(Window, [](GLFWwindow* window, int focused) {
		//	auto node = (DisplayOutNode*)glfwGetWindowUserPointer(window);
		//	if (focused == GLFW_FALSE && node->IsWindowLocked())
		//		glfwFocusWindow(window);
		//});
		glfwSetWindowCloseCallback(Window, [](GLFWwindow* window) {
			auto node = (DisplayOutNode*)glfwGetWindowUserPointer(window);
			if (node->IsWindowLocked())
				glfwSetWindowShouldClose(window, GLFW_FALSE);
		});

		glfwSetWindowPosCallback(Window, [](GLFWwindow* window, int posx, int posy)
			{
				auto node = (DisplayOutNode*)glfwGetWindowUserPointer(window);
				node->ResetWindowCurrentMonitorCache();
				if (node->IsWindowLocked())
				{
					if (auto monitor = node->GetGLFWMonitor())
					{
						int monitorPosX, monitorPosY;
						glfwGetMonitorPos(monitor, &monitorPosX, &monitorPosY);
						if (monitorPosX != posx || monitorPosY != posy)
						{
							glfwSetWindowPos(window, monitorPosX, monitorPosY);
							return;
						}
					}
					else // Monitor lost?
						node->RevertMonitorResolution();
				}
			});


		void* windowHandle = platform::GetVulkanWindowHandle(Window);
		if (nosVulkan->CreateWindowSurface(windowHandle, &Surface.GetStorage()) != NOS_RESULT_SUCCESS)
		{
			DestroyWindow();
			return;
		}
		TryCreateSwapchain();
		if (LockedMonitorPort)
			MoveToMonitor();
		if (IsCustomResolutionRequested())
			UpdateCustomResolution();
		if (Fullscreen)
			MakeFullscreen();
	}

	void OnPathStop() override
	{
		nosCmd cmd;
		nosCmdBeginParams beginParams = { .Name = NOS_NAME("Window node flush cmd"), .AssociatedNodeId = NodeId, .OutCmdHandle = &cmd };
		nosVulkan->Begin(&beginParams);
		nosGPUEvent wait;
		nosCmdEndParams endParams = { .ForceSubmit = true, .OutGPUEventHandle = &wait };
		nosVulkan->End(cmd, &endParams);
		nosVulkan->WaitGpuEvent(&wait, UINT64_MAX);
	}

	void OnPathStart() override
	{
		nosScheduleNodeParams params = {};
		params.NodeId = NodeId;
		params.Reset = false;
		params.AddScheduleCount = 1;

		nosEngine.ScheduleNode(&params);
	}

	void GetScheduleInfo(nosScheduleInfo* out) override
	{
		if (!VSync)
			return;

		float refreshRate = 60.0f;
		if (CustomResolutionActive)
		{
			refreshRate = RefreshRate;
		}
		else if (Window)
		{
			if (auto monitor = GetGLFWMonitor())
			{
				const GLFWvidmode* mode = glfwGetVideoMode(monitor);
				if (mode && mode->refreshRate > 0)
					refreshRate = float(mode->refreshRate);
			}
		}
		LastEffectiveRefreshRate = refreshRate;

		*out = nosScheduleInfo{
			.Importance = 1,
			.DeltaSeconds = {1000, static_cast<uint32_t>(1000.0f * refreshRate)},
			.Type = NOS_SCHEDULE_TYPE_ON_DEMAND,
		};
	}

	void OnPinValueChanged(nos::Name pinName, uuid const& pinId, nosBuffer value) override
	{
		platform::RunOnMainThread([&] { OnPinValueChangedMain(pinName, value); });
	}

	void OnPinValueChangedMain(nos::Name pinName, nosBuffer value)
	{
		if (pinName == NOS_NAME_STATIC("Resolution"))
		{
			Resolution = *InterpretObjectData<nosVec2u>(value);
			if (Window)
			{
				glfwSetWindowSize(Window, Resolution.x, Resolution.y);
			}
		}
		else if (pinName == NOS_NAME_STATIC("Fullscreen"))
		{
			Fullscreen = *InterpretObjectData<bool>(value);
			if (Window)
			{
				if (Fullscreen)
				{
					MakeFullscreen();
				}
				else
				{
					glfwSetWindowAttrib(Window, GLFW_DECORATED, GLFW_TRUE);
				}
			}
		}
		else if (pinName == NOS_NAME_STATIC("VSync"))
		{
			VSync = *InterpretObjectData<bool>(value);
			TryCreateSwapchain();
		}
		else if (pinName == NOS_NAME_STATIC("RefreshRate"))
		{
			RefreshRate = *InterpretObjectData<float>(value);
			if (IsCustomResolutionRequested())
				UpdateCustomResolution();
		}
		else if (pinName == NSN_Monitor)
		{
			const char* monitorName = InterpretObjectData<const char>(value);
			auto newPort = GetPortFromString(monitorName);
			if (newPort == LockedMonitorPort)
				return;
			if (CustomResolutionActive)
				RevertMonitorResolution();

			LockedMonitorPort = newPort;
			if (!LockedMonitorPort)
				return;
			MoveToMonitor();
			if (IsCustomResolutionRequested())
				UpdateCustomResolution();
		}
		else if (pinName == NOS_NAME_STATIC("ShowCursor"))
		{
			ShowCursor = *InterpretObjectData<bool>(value);
			if (Window)
				glfwSetInputMode(Window, GLFW_CURSOR, ShowCursor ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
		}
		else if (pinName == NOS_NAME_STATIC("WindowName"))
		{
			WindowName = InterpretObjectData<const char>(value);
			if(WindowName == "NONE")
				WindowName = std::nullopt;
			if (Window)
				glfwSetWindowTitle(Window, GetWindowName().c_str());
		}
	}

	void OnNodeUpdated(nosNodeUpdate const* update) override
	{
		if (WindowName)
			return;
		if (update->Type == NOS_NODE_UPDATE_DISPLAY_NAME || update->Type == NOS_NODE_UPDATE_UNIQUE_NAME)
			if (Window)
				platform::RunOnMainThread([&] { glfwSetWindowTitle(Window, GetWindowName().c_str()); });
	}

	void MoveToMonitor()
	{
		if (!Window)
		{
			nosEngine.LogE("Window not found");
			return;
		}
		if (auto monitor = GetGLFWMonitor())
		{
			int monitorPosX, monitorPosY;
			glfwGetMonitorPos(monitor, &monitorPosX, &monitorPosY);
			glfwSetWindowPos(Window, monitorPosX, monitorPosY);
		}
	}

	// Fallback port identifier used when there is no CustomResolutionBase
	// backend (e.g. macOS — NVAPI is Windows-only). We encode the
	// GLFWmonitor pointer into GPUId so the rest of the code can keep using
	// GPUPortIdentifier uniformly, and GetGLFWMonitor can decode it back.
	static GPUPortIdentifier PortFromGLFWMonitor(GLFWmonitor* monitor)
	{
		return GPUPortIdentifier{.GPUId = monitor, .PortId = 0};
	}

	std::optional<GPUPortIdentifier> GetWindowGPUPortId()
	{
		if(LockedMonitorPort.has_value())
			return *LockedMonitorPort;
		if (CachedMonitorPort.has_value())
			return *CachedMonitorPort;
		auto monitor = get_current_monitor(Window);
		if (!monitor)
			monitor = glfwGetWindowMonitor(Window);
		if (!monitor)
			return std::nullopt;
		if (auto customRes = CustomResolutionBase::Get())
		{
			const char* adapterName = platform::GetAdapterName(monitor);
			auto port = customRes->GetGPUPortIdFromAdapterName(adapterName);
			CachedMonitorPort = port;
			return port;
		}
		// No backend: fall back to identifying monitors by GLFW pointer.
		auto port = PortFromGLFWMonitor(monitor);
		CachedMonitorPort = port;
		return port;
	}

	GLFWmonitor* GetGLFWMonitor()
	{
		auto port = GetWindowGPUPortId();
		if (!port)
			return nullptr;
		// First try the port to monitor map
		if (auto it = PortToGLFWMonitor.find(*port); it != PortToGLFWMonitor.end())
		{
			return it->second;
		}
		if (auto* customRes = CustomResolutionBase::Get())
		{
			if (auto adapterName = customRes->GetAdapterName(*port, GetPossibleAdapterNames()))
				return GetGLFWMonitorFromAdapterName(adapterName->c_str());
			return nullptr;
		}
		// No backend: GPUId is the GLFWmonitor* itself.
		return static_cast<GLFWmonitor*>(port->GPUId);
	}

	void RequeryPortToGLFWMonitors()
	{
		PortToGLFWMonitor.clear();
		if (auto* customRes = CustomResolutionBase::Get())
		{
			auto activePorts = customRes->GetActivePortIds();
			for (auto& port : activePorts)
			{
				if (auto adapterName = customRes->GetAdapterName(port, GetPossibleAdapterNames()))
				{
					auto monitor = GetGLFWMonitorFromAdapterName(adapterName->c_str());
					if (monitor)
						PortToGLFWMonitor[port] = monitor;
				}
			}
			return;
		}
		// No backend: enumerate GLFW monitors directly.
		int count = 0;
		GLFWmonitor** monitors = glfwGetMonitors(&count);
		for (int i = 0; i < count; ++i)
			PortToGLFWMonitor[PortFromGLFWMonitor(monitors[i])] = monitors[i];
	}

	void ResetWindowCurrentMonitorCache()
	{ 
		CachedMonitorPort = std::nullopt;
	}

	void UpdateCustomResolution()
	{
		if (!CustomResolutionBase::Get())
		{
			nosEngine.LogE("CustomResolutionBase not found");
			return;
		}
		if (!Window)
		{
			nosEngine.LogE("Window not found");
			return;
		}
		auto monitor = GetWindowGPUPortId();
		if (!monitor)
		{
			nosEngine.LogE("Monitor not found");
			return;
		}
		CustomResolutionInfo info
		{
			.Resolution = Resolution,
			.RefreshRate = RefreshRate,
			.ColorDepth = ColorDepth,
			.ColorFormatBitDepth = ColorFormatBitDepth::Unorm32Bit
		};
		if (CustomResolutionActive)
			RevertMonitorResolution();
		if (CustomResolutionActive = CustomResolutionBase::Get()->SetResolutionAndRefreshRate(*monitor, info))
		{
			LockedMonitorPort = *monitor;
			UpdateMonitorString();
		}
	}

	void RevertMonitorResolution()
	{
		if (!CustomResolutionBase::Get())
		{
			nosEngine.LogE("CustomResolutionBase not found");
			return;
		}
		if (!Window)
		{
			nosEngine.LogE("Window not found");
			return;
		}
		if (CustomResolutionActive && LockedMonitorPort)
		{
			if (CustomResolutionBase::Get()->RevertResolution(*LockedMonitorPort))
			{
				CustomResolutionActive = false;
			}
		}
	}

	void MakeFullscreen()
	{
		auto* monitor = GetGLFWMonitor();
		if (!monitor)
		{
			nosEngine.LogE("Monitor not found");
			return;
		}
		auto mode = glfwGetVideoMode(monitor);
		int monitorPosX, monitorPosY;
		glfwGetMonitorPos(monitor, &monitorPosX, &monitorPosY);
		glfwSetWindowPos(Window, monitorPosX, monitorPosY);
		glfwSetWindowSize(Window, mode->width, mode->height);
		glfwSetWindowAttrib(Window, GLFW_DECORATED, GLFW_FALSE);
	}

	static nosResult GetFunctions(size_t* outCount, nosName* outFunctionNames, nosPfnNodeFunctionExecute* outFunction)
	{
		*outCount = 2;
		if (!outFunctionNames)
			return NOS_RESULT_SUCCESS;
		outFunctionNames[0] = NOS_NAME_STATIC("ForceUpdateMonitorResolution");
		outFunction[0] = [](void* ctx, nosFunctionExecuteParams* functionParams)
			{
				reinterpret_cast<DisplayOutNode*>(ctx)->SetPinValue(NSN_Internal_CustomResolutionRequested,
																nos::Buffer::From(true));
				reinterpret_cast<DisplayOutNode*>(ctx)->UpdateCustomResolution();
				return NOS_RESULT_SUCCESS;
			};
		outFunctionNames[1] = NOS_NAME_STATIC("RevertMonitorResolution");
		outFunction[1] = [](void* ctx, nosFunctionExecuteParams* functionParams)
			{
				reinterpret_cast<DisplayOutNode*>(ctx)->SetPinValue(NSN_Internal_CustomResolutionRequested, nos::Buffer::From(false));
				reinterpret_cast<DisplayOutNode*>(ctx)->RevertMonitorResolution();
				return NOS_RESULT_SUCCESS;
			};
		return NOS_RESULT_SUCCESS;
	}

	bool IsWindowLocked()
	{
		return CustomResolutionActive && Fullscreen;
	}

	std::string GetWindowName()
	{
		if (WindowName)
			return *WindowName;
		return GetDisplayName();
	}

	void UpdateMonitorString()
	{
		std::string monitorStr = "NONE";
		if (LockedMonitorPort)
			monitorStr = PortToString(*LockedMonitorPort);
		SetPinValue(NSN_Monitor, monitorStr.c_str());
	}

	std::string PortToString(GPUPortIdentifier port)
	{
		std::string displayDisplayName = "Unknown";
		if (auto* customRes = CustomResolutionBase::Get())
		{
			if (auto adapterName = customRes->GetAdapterName(port, GetPossibleAdapterNames()))
				if (auto monitor = GetGLFWMonitorFromAdapterName(adapterName->c_str()))
					displayDisplayName = glfwGetMonitorName(monitor);
		}
		else if (auto* monitor = static_cast<GLFWmonitor*>(port.GPUId))
		{
			// No CustomResolutionBase: GPUId is the GLFWmonitor* we want.
			if (const char* name = glfwGetMonitorName(monitor))
				displayDisplayName = name;
		}
		return displayDisplayName + " - " + std::to_string((uint64_t)port.GPUId) + " - " + std::to_string(port.PortId);
	}

	std::optional<GPUPortIdentifier> GetPortFromString(const char* portString)
	{
		std::string str = portString;
		// Go from the back to find the last '-'
		size_t portIdStart = str.find_last_of(" - ");
		if (portIdStart == std::string::npos)
			return std::nullopt;
		// Get the port id
		std::string portIdStr = str.substr(portIdStart + 1);
		std::stringstream ss(portIdStr);
		uint32_t portId;
		if(!(ss >> portId))
			return std::nullopt;
		// Get the GPU id
		size_t gpuIdStart = str.find_last_of(" - ", portIdStart - 3);
		if (gpuIdStart == std::string::npos)
			return std::nullopt;
		std::string gpuIdStr = str.substr(gpuIdStart + 1, portIdStart - 3 - gpuIdStart);
		std::stringstream ss2(gpuIdStr);
		uint64_t gpuId;
		if (!(ss2 >> gpuId))
			return std::nullopt;
		return GPUPortIdentifier{ .GPUId = (void*)gpuId, .PortId = portId };
	}

	std::vector<std::string> GetPossibleMonitors()
	{
		std::vector<std::string> monitors;
		monitors.push_back("NONE");
		if (auto* customRes = CustomResolutionBase::Get())
		{
			auto activePorts = customRes->GetActivePortIds();
			for (auto& port : activePorts)
				monitors.push_back(PortToString(port));
		}
		else
		{
			// No backend: enumerate GLFW monitors directly.
			int count = 0;
			GLFWmonitor** glfwMonitors = glfwGetMonitors(&count);
			for (int i = 0; i < count; ++i)
				monitors.push_back(PortToString(PortFromGLFWMonitor(glfwMonitors[i])));
		}
		return monitors;
	}

	bool IsCustomResolutionRequested()
	{
		auto val = GetWatchedPinValue<bool>(NSN_Internal_CustomResolutionRequested);
		if (!val)
			return false;
		return **val;
	}

	GLFWwindow* Window = nullptr;
	std::vector<TypedObjectRef<sys::vulkan::Semaphore>> WaitSemaphore{};
	std::vector<TypedObjectRef<sys::vulkan::Semaphore>> SignalSemaphore{};
	std::vector<nosGPUEvent> WaitEvents{};
	std::vector<TypedObjectRef<sys::vulkan::Texture>> Images{};
	uint32_t FrameCount = 0;
	uint32_t CurrentFrame = 0;
	TypedObjectRef<sys::vulkan::Surface> Surface{};
	TypedObjectRef<sys::vulkan::Swapchain> Swapchain{};

	nosVec2u Resolution = { 1920, 1080 };
	bool Fullscreen = false;
	bool VSync = false;
	float RefreshRate = 60.0f;
	float LastEffectiveRefreshRate = 0.0f;
	bool ShowCursor = false;
	std::optional<std::string> WindowName = std::nullopt;

	uint32_t ColorDepth = 32;
	nosFormat ColorFormat = NOS_FORMAT_B8G8R8A8_SRGB;

	bool CustomResolutionActive = false;
	std::optional<GPUPortIdentifier> LockedMonitorPort, CachedMonitorPort;

	std::unordered_map<GPUPortIdentifier, GLFWmonitor*> PortToGLFWMonitor;
};

nosResult RegisterDisplayOut(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("DisplayOut"), DisplayOutNode, fn);
	return NOS_RESULT_SUCCESS;
}
}
