//
// Created by Zach Lee on 2023/2/28.
//

#pragma once

#include <core/math/Color.h>
#include <framework/world/Component.h>

namespace sky {

    class LightComponent : public Component {
    public:
        LightComponent() = default;
        ~LightComponent() override = default;

        TYPE_RTTI_WITH_VT(LightComponent)

        static void Reflect();

        void Save(JsonOutputArchive &ar) const override;
        void Load(JsonInputArchive &ar) override;

    private:
        Color lightColor;
    };

} // namespace sky




