//
// Created by blues on 2024/3/3.
//

#pragma once

#include <xr/IXRPlatform.h>

namespace sky {

    class XRPlatformAndroid : public IXRPlatform {
    public:
        XRPlatformAndroid() = default;
        ~XRPlatformAndroid() override = default;

    private:
        const XrBaseInStructure *GetInstanceCreateInfo() const;
    };

} // namespace sky