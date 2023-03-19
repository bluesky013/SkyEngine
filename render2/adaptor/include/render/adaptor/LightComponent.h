//
// Created by Zach Lee on 2023/2/28.
//

#pragma once

#include <engine/base/Component.h>

namespace sky {

    class LightComponent : public Component {
    public:
        LightComponent() = default;
        ~LightComponent();

        TYPE_RTTI_WITH_VT(LightComponent, "C959A95E-83D0-4882-BBF4-707CF40BACDB")

        static void Reflect();

        void Save(JsonOutputArchive &ar) const override {}
        void Load(JsonInputArchive &ar) override {}
    };

} // namespace sky




