//
// Created by Zach Lee on 2022/9/25.
//

#include "Win32Platform.h"
static const char* TAG = "Win32Platform";

namespace sky {

    bool Platform::Init(const PlatformInfo& info)
    {
        platform = std::make_unique<Win32Platform>();
        return platform->Init(info);
    }
}