//
// Created by Zach Lee on 2022/9/25.
//

#include "Win32Platform.h"
static const char* TAG = "Win32Platform";

namespace sky {

    PlatformBase *PlatformBase::GetPlatform()
    {
        static Win32Platform platform;
        return &platform;
    }
}