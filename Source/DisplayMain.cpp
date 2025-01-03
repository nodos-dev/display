// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

// Includes
#include <Nodos/PluginHelpers.hpp>
#include <glm/glm.hpp>
#include <Builtins_generated.h>

#include <nosVulkanSubsystem/nosVulkanSubsystem.h>

#include "CustomResolutionBase.h"

NOS_INIT_WITH_MIN_REQUIRED_MINOR(13)
NOS_VULKAN_INIT()

NOS_BEGIN_IMPORT_DEPS()
NOS_VULKAN_IMPORT()
NOS_END_IMPORT_DEPS()

namespace nos::display
{

	enum Nodes : int
	{	// CPU nodes
		DisplayOut,
		Count
	};

	nosResult RegisterDisplayOut(nosNodeFunctions*);

	struct DisplayPluginFunctions : nos::PluginFunctions
	{
		nosResult Initialize() override
		{
			if (!CustomResolutionBase::Create() || CustomResolutionBase::Get()->Init())
				nosEngine.LogW("Failed to initialize CustomResolution!");
			return NOS_RESULT_SUCCESS;
		}
		nosResult OnPreUnloadPlugin() override
		{
			if (CustomResolutionBase::Get())
			{
				CustomResolutionBase::Get()->Shutdown();
				CustomResolutionBase::Destroy();
			}
			return NOS_RESULT_SUCCESS;
		}
		nosResult ExportNodeFunctions(size_t& outSize, nosNodeFunctions** outList) override
		{
			outSize = Nodes::Count;
			if (!outList)
				return NOS_RESULT_SUCCESS;

#define GEN_CASE_NODE(name)				\
	case Nodes::name: {					\
		auto ret = Register##name(node);	\
		if (NOS_RESULT_SUCCESS != ret)		\
			return ret;						\
		break;								\
	}

			for (int i = 0; i < Nodes::Count; ++i)
			{
				auto node = outList[i];
				switch ((Nodes)i) {
				default:
					break;
					GEN_CASE_NODE(DisplayOut)
				}
			}
			return NOS_RESULT_SUCCESS;
		}
	};
	NOS_EXPORT_PLUGIN_FUNCTIONS(DisplayPluginFunctions)
}
