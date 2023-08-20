//
// Created by Zach Lee on 2023/2/28.
//

#pragma once

#include <framework/world/component.h>
#include <render/assets/Mesh.h>

namespace sky {

    class MeshComponent : public Component {
    public:
        MeshComponent() = default;
        ~MeshComponent() override = default;

        TYPE_RTTI_WITH_VT(MeshComponent)

        static void Reflect();

        void Save(JsonOutputArchive &ar) const override;
        void Load(JsonInputArchive &ar) override;

    private:
        MeshAssetPtr mesh;
    };

} // namespace sky