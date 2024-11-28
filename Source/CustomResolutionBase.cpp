#include "CustomResolutionBase.h"

namespace nos::display
{
std::unique_ptr<CustomResolutionBase> CustomResolutionBase::Instance = nullptr;

extern std::unique_ptr<CustomResolutionBase> TryCreateNVIDIACustomResolution();


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