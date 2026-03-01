//
// Created by blues on 2023/10/13.
//

#pragma once

#include <rhi/Core.h>

namespace sky::rhi {

    class PipelineLibrary {
    public:
        PipelineLibrary() = default;
        virtual ~PipelineLibrary() = default;

        struct Descriptor {
            bool externalSynchronized = true;
            uint32_t dataSize = 0;
            const char* data = nullptr;
        };
    };

} // namespace sky::rhi