//
// Created by blues on 2024/3/3.
//

#pragma once

#include <openxr/openxr.h>
#include <vector>
#include <string>

namespace sky {

    class IXRPlatform {
    public:
        IXRPlatform() = default;
        virtual ~IXRPlatform() = default;

        virtual const XrBaseInStructure *GetInstanceCreateInfo() const { return nullptr; }
    };

    IXRPlatform *CreateXRPlatform();

} // namespace sky