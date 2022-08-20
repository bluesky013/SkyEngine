//
// Created by Zach Lee on 2022/8/20.
//

#pragma once

#include <cstdint>
#include <core/util/Macros.h>

namespace sky {
    class RenderRasterEncoder;

    class RenderPrimitive {
    public:
        RenderPrimitive() = default;

        virtual ~RenderPrimitive() = default;

        SKY_DISABLE_COPY(RenderPrimitive);

        inline void SetViewTag(uint32_t tag)
        {
            viewMask |= tag;
        }

        inline uint32_t GetViewMask() const
        {
            return viewMask;
        }

        virtual void Encode(RenderRasterEncoder*) {}

    protected:
        uint32_t viewMask = 0;
    };
}