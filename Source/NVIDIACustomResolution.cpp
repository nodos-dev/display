#include "CustomResolutionBase.h"

#include <nvapi/nvapi.h>


namespace nos::display
{
std::string GetErrorString(NvAPI_Status status)
{
	NvAPI_ShortString errorString;
	NvAPI_GetErrorMessage(status, errorString);
	return errorString;
}

struct NVIDIACustomResolution : CustomResolutionBase
{
	bool Init() override
	{
		if (auto err = NvAPI_Initialize(); err != NVAPI_OK)
		{
			nosEngine.LogE("Failed to initialize NVAPI: %s", GetErrorString(err).c_str());
			return false;
		}
		return true;
	}

	void Shutdown() override
	{
		NvAPI_Unload();
	}

	std::optional<NvU32> GetDisplayIdFromPort(GPUPortIdentifier portId)
	{
		NvU32 displayId = 0;
		
		if (auto err = NvAPI_SYS_GetDisplayIdFromGpuAndOutputId((NvPhysicalGpuHandle)portId.GPUId, portId.PortId, &displayId); err != NVAPI_OK)
		{
			nosEngine.LogE("Failed to get display ID from port: %s", GetErrorString(err).c_str());
			return std::nullopt;
		}
		return displayId;
	}

	bool SetResolutionAndRefreshRate(GPUPortIdentifier portId, CustomResolutionInfo info) override
	{
		auto dispId = GetDisplayIdFromPort(portId);
		if (!dispId)
			return false;
		NV_TIMING_INPUT timing{ .version = NV_TIMING_INPUT_VER };
		timing.rr = info.RefreshRate;
		timing.width = info.Resolution.x;
		timing.height = info.Resolution.y;
		timing.flag.scaling = 1;
		timing.type = NV_TIMING_OVERRIDE_AUTO;
		NV_CUSTOM_DISPLAY customDisplay{ .version = NV_CUSTOM_DISPLAY_VER };
		if (auto err = NvAPI_DISP_GetTiming(dispId.value(), &timing, &customDisplay.timing); err != NVAPI_OK)
		{
			nosEngine.LogE("Failed to get timing: %s", GetErrorString(err).c_str());
			return false;
		}
		customDisplay.width = info.Resolution.x;
		customDisplay.height = info.Resolution.y;
		customDisplay.depth = info.ColorDepth;
		switch (info.ColorFormat)
		{
			case NOS_FORMAT_B8G8R8A8_UNORM:
				customDisplay.colorFormat = NV_FORMAT_A8R8G8B8;
				break;
			default:
				nosEngine.LogE("Unsupported color format: %d", info.ColorFormat);
				return false;
		}
		customDisplay.srcPartition.x = 0;
		customDisplay.srcPartition.y = 0;
		customDisplay.srcPartition.h = 1;
		customDisplay.srcPartition.w = 1;
		customDisplay.xRatio = 1;
		customDisplay.yRatio = 1;

		if (auto err = NvAPI_DISP_TryCustomDisplay(&dispId.value(), 1, &customDisplay); err != NVAPI_OK)
		{
			nosEngine.LogE("Failed to try custom display: %s", GetErrorString(err).c_str());
			return false;
		}

		if (auto err = NvAPI_DISP_SaveCustomDisplay(&dispId.value(), 1, true, true); err != NVAPI_OK)
		{
			RevertResolution(portId);
			nosEngine.LogE("Failed to save custom display: %s", GetErrorString(err).c_str());
			return false;
		}
		return true;
	}

	bool RevertResolution(GPUPortIdentifier portId) override
	{
		auto dispId = GetDisplayIdFromPort(portId);
		if (!dispId)
			return false;
		if (auto err = NvAPI_DISP_RevertCustomDisplayTrial(&dispId.value(), 1); err != NVAPI_OK)
		{
			nosEngine.LogE("Failed to revert custom display: %s", GetErrorString(err).c_str());
			return false;
		}
		return true;
	}

	std::optional<std::string> GetAdapterName(GPUPortIdentifier portId, std::vector<std::string> const& possibleAdapterNames) override
	{
		auto id = GetDisplayIdFromPort(portId);
		if (!id)
			return std::nullopt;
		for (auto& adapterName : possibleAdapterNames)
		{
			NvU32 curDispId{};
			if(auto err = NvAPI_DISP_GetDisplayIdByDisplayName(adapterName.c_str(), &curDispId); err != NVAPI_OK)
				continue;
			if (id.value() == curDispId)
				return adapterName;
		}
		return std::nullopt;
	}

	std::optional<GPUPortIdentifier> GetGPUPortIdFromAdapterName(const char* adapterName) override
	{
		NvU32 displayId{};
		if (auto err = NvAPI_DISP_GetDisplayIdByDisplayName(adapterName, &displayId); err != NVAPI_OK)
		{
			nosEngine.LogE("Failed to get display id by display name: %s", GetErrorString(err).c_str());
			return std::nullopt;
		}
		NvPhysicalGpuHandle gpuHandle{};
		NvU32 portId{};
		if (auto err = NvAPI_SYS_GetGpuAndOutputIdFromDisplayId(displayId, &gpuHandle, &portId); err != NVAPI_OK)
		{
			nosEngine.LogE("Failed to get gpu and port id from display id: %s", GetErrorString(err).c_str());
			return std::nullopt;
		}
		return GPUPortIdentifier{ .GPUId = gpuHandle, .PortId = portId };
	}

	std::vector<GPUPortIdentifier> GetActivePortIds()
	{
		NvPhysicalGpuHandle nvGPUHandle[NVAPI_MAX_PHYSICAL_GPUS]{};
		NvU32 gpuCount = 0;

		// Get all the Physical GPU Handles
		if (auto err = NvAPI_EnumPhysicalGPUs(nvGPUHandle, &gpuCount); err != NVAPI_OK)
		{
			nosEngine.LogE("Failed to enumerate physical GPUs: %s", GetErrorString(err).c_str());
			return {};
		}

		std::vector<GPUPortIdentifier> result;
		for (uint32_t gpuIndex = 0; gpuIndex < gpuCount; gpuIndex++)
		{
			NvU32 dispIdCount = 0;
			if (auto err = NvAPI_GPU_GetConnectedDisplayIds(nvGPUHandle[gpuIndex], NULL, &dispIdCount, 0); err != NVAPI_OK)
			{
				nosEngine.LogE("Failed to get connected display id count: %s", GetErrorString(err).c_str());
				continue;
			}

			std::vector<NV_GPU_DISPLAYIDS> dispIds(dispIdCount);

			for (NvU32 dispIndex = 0; dispIndex < dispIdCount; dispIndex++)
			{
				dispIds[dispIndex].version = NV_GPU_DISPLAYIDS_VER; // adding the correct version information
			}

			if (auto err = NvAPI_GPU_GetConnectedDisplayIds(nvGPUHandle[gpuIndex], dispIds.data(), &dispIdCount, 0); err != NVAPI_OK)
			{
				nosEngine.LogE("Failed to get connected display ids: %s", GetErrorString(err).c_str());
				continue;
			}

			for (auto& dispId : dispIds)
			{
				if (!dispId.isActive)
					continue;
				NvPhysicalGpuHandle gpuHandle{};
				NvU32 portId{};
				if (auto err = NvAPI_SYS_GetGpuAndOutputIdFromDisplayId(dispId.displayId, &gpuHandle, &portId))
				{
					nosEngine.LogE("Failed to get gpu and port id: %s", GetErrorString(err).c_str());
					continue;
				}
				result.push_back(GPUPortIdentifier{ .GPUId = gpuHandle, .PortId = portId });
			}
		}
		return result;
	}
};

std::unique_ptr<CustomResolutionBase> TryCreateNVIDIACustomResolution()
{
	return std::make_unique<NVIDIACustomResolution>();
}
}