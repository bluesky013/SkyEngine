//
// Created by Zach Lee on 2023/2/28.
//

#pragma once

#include <framework/world/Component.h>
#include <render/adaptor/assets/MeshAsset.h>
#include <render/resource/Mesh.h>
#include <render/mesh/StaticMeshRenderer.h>

namespace sky {

    class MeshRenderer : public Component {
    public:
        MeshRenderer() = default;
        ~MeshRenderer() override = default;

        TYPE_RTTI_WITH_VT(MeshRenderer)

        static void Reflect();

        void OnActive() override;
        void OnDestroy() override;
        void OnTick(float time) override;

        void SetMesh(const MeshAssetPtr &mesh);

        void Save(JsonOutputArchive &ar) const override;
        void Load(JsonInputArchive &ar) override;

    private:
        void ResetMesh();

        bool isStatic = true;
        bool castShadow = false;
        bool receiveShadow = false;

        MeshAssetPtr meshAsset;
        StaticMeshRenderer *renderer = nullptr;
    };

} // namespace receiveShadow
