//
// Created by blues on 2024/3/3.
//

#include "XRPlatformAndroid.h"

namespace sky {

    const XrBaseInStructure *XRPlatformAndroid::GetInstanceCreateInfo() const
    {
        return nullptr;
    }

    IXRPlatform *CreateXRPlatform()
    {
        return new XRPlatformAndroid();
    }

} // namespace sky