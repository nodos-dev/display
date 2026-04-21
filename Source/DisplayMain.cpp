// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

// Includes
#include <Nodos/Plugin.hpp>
#include <glm/glm.hpp>
#include <Builtins_generated.h>

#include <nosSysVulkan/nosVulkanSubsystem.h>

#include "CustomResolutionBase.h"
#include "Platform.h"
#include "GLFW/glfw3.h"

NOS_INIT()
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
		bool glfwInitialized = false;
		nosResult Initialize() override
		{
			// GLFW's own docs require glfwInit on the main thread, and on
			// macOS it creates [NSApplication sharedApplication] which is a
			// hard AppKit/main-thread requirement.
			bool ok = false;
			platform::RunOnMainThread([&] { ok = glfwInit() == GLFW_TRUE; });
			if (!ok)
			{
				nosEngine.LogE("Failed to initialize GLFW");
				return NOS_RESULT_FAILED;
			}
			glfwInitialized = true;
			if (!CustomResolutionBase::Create() || !CustomResolutionBase::Get()->Init())
				nosEngine.LogW("Failed to initialize CustomResolution!");
			return NOS_RESULT_SUCCESS;
		}
		nosResult OnPreUnloadPlugin() override
		{
			if (glfwInitialized)
			{
				platform::RunOnMainThread([] { glfwTerminate(); });
				glfwInitialized = false;
			}
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
