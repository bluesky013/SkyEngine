//
// Created by blues on 2024/3/3.
//

#include "XRPlatformWin32.h"
#include <openxr/openxr_platform.h>

namespace sky {

    IXRPlatform *CreateXRPlatform()
    {
        return new XRPlatformWin32();
    }

} // namespace sky