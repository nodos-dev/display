#pragma once

#include <Nodos/PluginHelpers.hpp>
#include <nosVulkanSubsystem/nosVulkanSubsystem.h>

namespace nos::display
{
struct CustomResolutionInfo
{
	nosVec2u Resolution;
	float RefreshRate;
	uint32_t ColorDepth = 32;
	nosFormat ColorFormat = NOS_FORMAT_B8G8R8A8_UNORM;
};

struct GPUPortIdentifier
{
	void* GPUId;
	uint32_t PortId;
	auto operator<=>(const GPUPortIdentifier&) const = default;
};

struct CustomResolutionBase
{
	virtual ~CustomResolutionBase() = default;
	static bool Create();
	static void Destroy();
	virtual bool Init() = 0;
	virtual void Shutdown() = 0;
	virtual bool SetResolutionAndRefreshRate(GPUPortIdentifier portId, CustomResolutionInfo info) = 0;
	virtual bool RevertResolution(GPUPortIdentifier portId) = 0;
	virtual std::optional<std::string> GetAdapterName(GPUPortIdentifier portId, std::vector<std::string> const& possibleAdapterNames) = 0;
	virtual std::vector<GPUPortIdentifier> GetActivePortIds() = 0;
	virtual std::optional<GPUPortIdentifier> GetGPUPortIdFromAdapterName(const char* adapterName) = 0;

	static CustomResolutionBase* Get();
private:
	static std::unique_ptr<CustomResolutionBase> Instance;
};
}