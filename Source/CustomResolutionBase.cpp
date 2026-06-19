#include "CustomResolutionBase.h"

namespace nos::display
{
std::unique_ptr<CustomResolutionBase> CustomResolutionBase::Instance = nullptr;

#if defined(_WIN32)
extern std::unique_ptr<CustomResolutionBase> TryCreateNVIDIACustomResolution();
#else
// NVAPI is Windows-only; custom resolution support is not available elsewhere.
static std::unique_ptr<CustomResolutionBase> TryCreateNVIDIACustomResolution() { return nullptr; }
#endif

bool CustomResolutionBase::Create()
{
	Instance = TryCreateNVIDIACustomResolution();
	if (!Instance)
		return false;
	return true;
}

void CustomResolutionBase::Destroy()
{
	Instance.reset();
}

CustomResolutionBase* CustomResolutionBase::Get()
{
	return Instance.get();
}

}