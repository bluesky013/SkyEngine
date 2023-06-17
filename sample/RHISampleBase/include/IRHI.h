//
// Created by Zach Lee on 2023/4/7.
//

#pragma once

#include <framework/interface/Interface.h>

namespace sky::rhi {
    class Device;

    struct IRHI {
        virtual rhi::Device * GetDevice() const = 0;
    };

}