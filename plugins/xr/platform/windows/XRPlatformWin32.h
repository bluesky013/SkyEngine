//
// Created by blues on 2024/3/3.
//

#pragma once

#include <xr/IXRPlatform.h>
#include <windows.h>

namespace sky {

    class XRPlatformWin32 : public IXRPlatform {
    public:
        XRPlatformWin32() { CoInitializeEx(nullptr, COINIT_MULTITHREADED);}
        ~XRPlatformWin32() override = default;
    };

} // namespace sky