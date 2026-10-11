// generated: do not edit
#pragma once

#include <cstddef>
#include <cstdint>
#include <core/math/Matrix4.h>
#include <core/math/Vector2.h>
#include <aurora/shader/RgBlockDesc.h>

namespace sky::aurora::generated {

    struct GlobalParams {
        alignas(16) Matrix4 view;
        alignas(16) Matrix4 proj;
        alignas(16) Matrix4 viewProj;
        alignas(16) Vector4 cameraPos;
    };

    static_assert(sizeof(GlobalParams) == 208, "GlobalParams size mismatch");
    static_assert(offsetof(GlobalParams, view) == 0, "GlobalParams.view offset mismatch");
    static_assert(offsetof(GlobalParams, proj) == 64, "GlobalParams.proj offset mismatch");
    static_assert(offsetof(GlobalParams, viewProj) == 128, "GlobalParams.viewProj offset mismatch");
    static_assert(offsetof(GlobalParams, cameraPos) == 192, "GlobalParams.cameraPos offset mismatch");

    inline const RgBlockDesc &GetGlobalParamsBlockDesc()
    {
        static const RgBlockDesc desc = [] {
            RgBlockDesc d{};
            d.set       = 0;
            d.binding   = 0;
            d.blockName = Name("GlobalParams");
            d.kind      = RgBlockKind::CBUFFER;
            d.fields    = {
                { RgFieldType::MAT4, Name("view") },
                { RgFieldType::MAT4, Name("proj") },
                { RgFieldType::MAT4, Name("viewProj") },
                { RgFieldType::FLOAT4, Name("cameraPos") },
            };
            return d;
        }();
        return desc;
    }

} // namespace sky::aurora::generated

