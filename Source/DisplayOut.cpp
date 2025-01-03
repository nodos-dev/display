#include "CustomResolutionBase.h"

#include <Nodos/PluginHelpers.hpp>
#include <nosVulkanSubsystem/Helpers.hpp>

#include "nosUtil/Stopwatch.hpp"
#include "GLFW/glfw3.h"
#if defined(WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux)
#define GLFW_EXPOSE_NATIVE_X11
#else
#error "Unsupported platform"
#endif
#include "GLFW/glfw3native.h"


namespace nos::display
{

std::vector<std::string> GetPossibleAdapterNames()
{
	std::vector<std::string> adapterNames;
	int monitorCount;
	GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
	for (int i = 0; i < monitorCount; i++)
		adapterNames.push_back(glfwGetWin32Adapter(monitors[i]));
	return adapterNames;
}

GLFWmonitor* GetGLFWMonitorFromAdapterName(const char* adapterName)
{
	int monitorCount;
	GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
	for (int i = 0; i < monitorCount; i++)
	{
		const char* name = glfwGetWin32Adapter(monitors[i]);
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
		const char* name = glfwGetWin32Adapter(monitors[i]);
		if (strcmp(name, monitorName) == 0)
			return monitors[i];
	}
	return nullptr;
}

NOS_REGISTER_NAME(Monitor)

struct DisplayOutNode : NodeContext
{
	DisplayOutNode(const fb::Node* node) : NodeContext(node)
	{
		fb::TVisualizer visualizer;
		visualizer.type = fb::VisualizerType::COMBO_BOX;
		visualizer.name = std::string("Monitor_") + UUID2STR(NodeId);
		SetPinVisualizer(NSN_Monitor, visualizer);
		UpdateStringList(std::string("Monitor_") + UUID2STR(NodeId), {"NONE"});
	}

	~DisplayOutNode()
	{
		Clear();
	}

	bool CreateSwapchain()
	{
		nosSwapchainCreateInfo createInfo = {};
		createInfo.SurfaceHandle = Surface;
		// get extent
		int width, height;
		glfwGetWindowSize(Window, &width, &height);
		createInfo.Extent = { uint32_t(width), uint32_t(height) };
		createInfo.PresentMode = VSync ? NOS_PRESENT_MODE_FIFO : NOS_PRESENT_MODE_IMMEDIATE;
		nosResult res = nosVulkan->CreateSwapchain(&createInfo, &Swapchain, &FrameCount);
		if (res != NOS_RESULT_SUCCESS)
			return false;
		nosSemaphoreCreateInfo semaphoreCreateInfo = {};
		semaphoreCreateInfo.Type = NOS_SEMAPHORE_TYPE_BINARY;
		Images.resize(FrameCount);
		nosVulkan->GetSwapchainImages(Swapchain, Images.data());
		WaitSemaphore.resize(FrameCount);
		SignalSemaphore.resize(FrameCount);
		for (int i = 0; i < FrameCount; i++)
		{
#ifdef CreateSemaphore
#undef CreateSemaphore
#endif
			nosVulkan->CreateSemaphore(&semaphoreCreateInfo, &WaitSemaphore[i]);
			nosVulkan->CreateSemaphore(&semaphoreCreateInfo, &SignalSemaphore[i]);
		}
		return true;
	}

	void Clear()
	{
		if(CustomResolutionSet)
			RevertMonitorResolution(false);
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
			DestroyWindowSurface();
			DestroyWindow();
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
		nosVulkan->Begin2(&beginParams);
		nosGPUEvent wait;
		nosCmdEndParams endParams = { .ForceSubmit = true, .OutGPUEventHandle = &wait };
		nosVulkan->End(cmd, &endParams);
		nosVulkan->WaitGpuEvent(&wait, UINT64_MAX);
		for (int i = 0; i < FrameCount; i++)
		{
			nosVulkan->DestroySemaphore(&WaitSemaphore[i]);
			nosVulkan->DestroySemaphore(&SignalSemaphore[i]);
		}
		WaitSemaphore.clear();
		SignalSemaphore.clear();
		Images.clear();
		nosVulkan->DestroySwapchain(&Swapchain);
	}

	void DestroyWindowSurface()
	{
		if (!Surface)
			return;
		nosVulkan->DestroyWindowSurface(&Surface);
	}

	void DestroyWindow()
	{
		if (!Window)
			return;
		glfwDestroyWindow(Window);
		glfwTerminate();
		Window = nullptr;
	}

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		if (!Window)
			return NOS_RESULT_FAILED;
		nosScheduleNodeParams scheduleParams = {};
		scheduleParams.NodeId = NodeId;
		scheduleParams.Reset = false;
		scheduleParams.AddScheduleCount = 1;

		nos::NodeExecuteParams execParams = params;

		auto input = vkss::DeserializeTextureInfo(execParams[NOS_NAME("Input")].Data->Data);
		if (!input.Memory.Handle)
			return NOS_RESULT_FAILED;

		if (!glfwWindowShouldClose(Window))
		{
			glfwPollEvents();
			const char* errDesc;
			int err = glfwGetError(&errDesc);
			if (err != GLFW_NO_ERROR)
			{
				nosEngine.LogE("Error: %s", errDesc);
				return NOS_RESULT_FAILED;
			}

			uint32_t imageIndex;
			nosVulkan->SwapchainAcquireNextImage(Swapchain, -1, &imageIndex, WaitSemaphore[CurrentFrame]);
			nosCmd cmd;
			nosVulkan->Begin("Window", &cmd);
			nosVulkan->Copy(cmd, &input, &Images[imageIndex], 0);

			nosVulkan->ImageStateToPresent(cmd, &Images[imageIndex]);
			nosVulkan->AddWaitSemaphoreToCmd(cmd, WaitSemaphore[CurrentFrame], 1);
			nosVulkan->AddSignalSemaphoreToCmd(cmd, SignalSemaphore[CurrentFrame], 1);

			nosCmdEndParams endParams{ .ForceSubmit = true };
			nosVulkan->End(cmd, &endParams);
			if (nosVulkan->SwapchainPresent(Swapchain, imageIndex, SignalSemaphore[CurrentFrame]) != NOS_RESULT_SUCCESS)
			{
				TryCreateSwapchain();
			}
			nosEngine.ScheduleNode(&scheduleParams);
			CurrentFrame = (CurrentFrame + 1) % FrameCount;
		}
		else
		{
			Clear();
			return NOS_RESULT_FAILED;
		}

		return NOS_RESULT_SUCCESS;
	}

	void OnExitRunnerThread(std::optional<nosUUID> runnerId) override
	{
		if (!runnerId)
			return;
		Clear();
	}

	void OnEnterRunnerThread(std::optional<nosUUID> runnerId) override
	{
		if (!runnerId)
			return;
		glfwInit();
		UpdateStringList(std::string("Monitor_") + UUID2STR(NodeId), GetPossibleMonitors());
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		Window = glfwCreateWindow(Resolution.x, Resolution.y, "NOSWindow", nullptr, nullptr);
		glfwSetWindowUserPointer(Window, this);
		glfwSetWindowSizeCallback(Window, [](GLFWwindow* window, int width, int height) {
			auto node = (DisplayOutNode*)glfwGetWindowUserPointer(window);
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
						node->RevertMonitorResolution(false);
				}
				else
				{
					node->TryCreateSwapchain();
					return;
				}
			}
			node->TryCreateSwapchain();
		});
		glfwSetWindowIconifyCallback(Window, [](GLFWwindow* window, int iconified) {
			auto node = (DisplayOutNode*)glfwGetWindowUserPointer(window);
			if (iconified == GLFW_TRUE && node->IsWindowLocked())
				glfwRestoreWindow(window);
		});
		//glfwSetWindowFocusCallback(Window, [](GLFWwindow* window, int focused) {
		//	auto node = (DisplayOutNode*)glfwGetWindowUserPointer(window);
		//	if (focused == GLFW_FALSE && node->IsWindowLocked())
		//		glfwFocusWindow(window);
		//});
		glfwSetWindowCloseCallback(Window, [](GLFWwindow* window) {
			auto node = (DisplayOutNode*)glfwGetWindowUserPointer(window);
			if(node->IsWindowLocked())
				glfwSetWindowShouldClose(window, GLFW_FALSE);
		});

		glfwSetWindowPosCallback(Window, [](GLFWwindow* window, int posx, int posy)
			{
				auto node = (DisplayOutNode*)glfwGetWindowUserPointer(window);
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
						node->RevertMonitorResolution(false);
				}
			});


		auto windowHandle =
#if defined(WIN32)
			glfwGetWin32Window(Window)
#elif defined(__linux)
			glfwGetX11Window(Window)
#else
#error "Unsupported platform"
#endif
			;
		if (nosVulkan->CreateWindowSurface((void*)windowHandle, &Surface) != NOS_RESULT_SUCCESS)
		{
			DestroyWindow();
			return;
		}
		TryCreateSwapchain();
		if (LockedMonitorPort)
		{
			MoveToMonitor();
			UpdateCustomResolution();
		}
		if (Fullscreen)
			MakeFullscreen();
	}

	void OnPathStop() override
	{
		nosCmd cmd;
		nosCmdBeginParams beginParams = { .Name = NOS_NAME("Window node flush cmd"), .AssociatedNodeId = NodeId, .OutCmdHandle = &cmd };
		nosVulkan->Begin2(&beginParams);
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

	void OnPinValueChanged(nos::Name pinName, nosUUID pinId, nosBuffer value) override
	{
		if (pinName == NOS_NAME_STATIC("Resolution"))
		{
			Resolution = *InterpretPinValue<nosVec2u>(value);
			if (Window)
			{
				glfwSetWindowSize(Window, Resolution.x, Resolution.y);
			}
		}
		else if (pinName == NOS_NAME_STATIC("Fullscreen"))
		{
			Fullscreen = *InterpretPinValue<bool>(value);
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
			VSync = *InterpretPinValue<bool>(value);
			TryCreateSwapchain();
		}
		else if (pinName == NOS_NAME_STATIC("RefreshRate"))
		{
			RefreshRate = *InterpretPinValue<float>(value);
			if (CustomResolutionSet)
				UpdateCustomResolution();
		}
		else if (pinName == NSN_Monitor)
		{
			const char* monitorName = InterpretPinValue<const char>(value);
			if (strcmp(monitorName, "NONE") == 0 || strlen(monitorName) == 0)
				return;
			auto newPort = GetPortFromString(monitorName);
			if (newPort == LockedMonitorPort)
				return;
			bool customResolutionWasSet = CustomResolutionSet;
			if (CustomResolutionSet)
			{
				RevertMonitorResolution(false);
			}

			LockedMonitorPort = newPort;
			if (!LockedMonitorPort)
				return;
			MoveToMonitor();
			if (customResolutionWasSet)
				UpdateCustomResolution();
		}
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

	std::optional<GPUPortIdentifier> GetWindowGPUPortId()
	{
		if(LockedMonitorPort.has_value())
			return *LockedMonitorPort;
		auto monitor = get_current_monitor(Window);
		if (!monitor)
			monitor = glfwGetWindowMonitor(Window);
		if (!monitor)
			return std::nullopt;
#if defined(WIN32)
		const char* adapterName = glfwGetWin32Adapter(monitor);
#endif
		if (auto customRes = CustomResolutionBase::Get())
			return customRes->GetGPUPortIdFromAdapterName(adapterName);
		return std::nullopt;
	}

	GLFWmonitor* GetGLFWMonitor()
	{
		auto port = GetWindowGPUPortId();
		if (!port)
			return nullptr;
		if(auto adapterName = CustomResolutionBase::Get()->GetAdapterName(*port, GetPossibleAdapterNames()))
			return GetGLFWMonitorFromAdapterName(adapterName->c_str());
		return nullptr;
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
			.ColorFormat = ColorFormat
		};
		if(CustomResolutionSet)
			RevertMonitorResolution(true);
		if (CustomResolutionSet = CustomResolutionBase::Get()->SetResolutionAndRefreshRate(*monitor, info))
		{
			LockedMonitorPort = *monitor;
			UpdateMonitorString();
		}
	}

	void RevertMonitorResolution(bool setMonitorPin)
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
		if (CustomResolutionSet && LockedMonitorPort)
		{
			if (CustomResolutionBase::Get()->RevertResolution(*LockedMonitorPort))
			{
				CustomResolutionSet = false;
				if (setMonitorPin)
				{
					LockedMonitorPort = std::nullopt;
					UpdateMonitorString();
				}
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
				reinterpret_cast<DisplayOutNode*>(ctx)->UpdateCustomResolution();
				return NOS_RESULT_SUCCESS;
			};
		outFunctionNames[1] = NOS_NAME_STATIC("RevertMonitorResolution");
		outFunction[1] = [](void* ctx, nosFunctionExecuteParams* functionParams)
			{
				reinterpret_cast<DisplayOutNode*>(ctx)->RevertMonitorResolution(true);
				return NOS_RESULT_SUCCESS;
			};
		return NOS_RESULT_SUCCESS;
	}

	bool IsWindowLocked()
	{
		return CustomResolutionSet && Fullscreen;
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
		// Get windows adapter name
		std::string displayDisplayName = "Unknown";
		if (auto adapterName = CustomResolutionBase::Get()->GetAdapterName(port, GetPossibleAdapterNames()))
			if(auto monitor = GetGLFWMonitorFromAdapterName(adapterName->c_str()))
				displayDisplayName = glfwGetMonitorName(monitor);
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
		auto activePorts = CustomResolutionBase::Get()->GetActivePortIds();
		std::vector<std::string> monitors;
		for (auto& port : activePorts)
			monitors.push_back(PortToString(port));
		return monitors;
	}

	GLFWwindow* Window = nullptr;
	std::vector<nosSemaphore> WaitSemaphore{};
	std::vector<nosSemaphore> SignalSemaphore{};
	std::vector<nosResourceShareInfo> Images{};
	uint32_t FrameCount = 0;
	uint32_t CurrentFrame = 0;
	nosSurfaceHandle Surface{};
	nosSwapchainHandle Swapchain{};

	nosVec2u Resolution = { 1920, 1080 };
	bool Fullscreen = false;
	bool VSync = false;
	float RefreshRate = 60.0f;
	uint64_t MonitorId;

	uint32_t ColorDepth = 32;
	nosFormat ColorFormat = NOS_FORMAT_B8G8R8A8_UNORM;

	bool CustomResolutionSet = false;
	std::optional<GPUPortIdentifier> LockedMonitorPort;
};

nosResult RegisterDisplayOut(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("DisplayOut"), DisplayOutNode, fn);
	return NOS_RESULT_SUCCESS;
}
}