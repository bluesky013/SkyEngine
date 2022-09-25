//
// Created by Zach Lee on 2022/9/25.
//

#include "MacosPlatform.h"
static const char* TAG = "Win32Platform";

namespace sky {

    PlatformBase *PlatformBase::GetPlatform()
    {
        static MacosPlatform platform;
        return &platform;
    }
}