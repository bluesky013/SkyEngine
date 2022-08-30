//
// Created by Zach Lee on 2022/1/31.
//

#pragma once

#include <engine/world/Component.h>

namespace sky {

    class MeshComponent : public Component {
    public:
        MeshComponent()  = default;
        ~MeshComponent() = default;

        TYPE_RTTI_WITH_VT(MeshComponent)

        void OnTick(float time) override;

        static void Reflect();
    };

} // namespace sky