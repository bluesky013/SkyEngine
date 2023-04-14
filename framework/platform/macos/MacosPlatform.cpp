//
// Created by Zach Lee on 2022/9/25.
//

#include "MacosPlatform.h"
static const char* TAG = "Win32Platform";

namespace sky {

    bool Platform::Init(const PlatformInfo& info)
    {
        platform = std::make_unique<MacosPlatform>();
        return platform->Init(info);
    }
}