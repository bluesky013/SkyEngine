//
// Created by Zach Lee on 2023/2/28.
//

#pragma once

#include <engine/base/Component.h>
#include <render/assets/Mesh.h>

namespace sky {

    class MeshComponent : public Component {
    public:
        MeshComponent() = default;
        ~MeshComponent();

        TYPE_RTTI_WITH_VT(MeshComponent, "EF68148A-907C-4D99-A401-CBEC6F939949")

        static void Reflect();

        void Save(JsonOutputArchive &ar) const override {}
        void Load(JsonInputArchive &ar) override {}

    private:
        MeshAssetPtr mesh;
    };

} // namespace sky