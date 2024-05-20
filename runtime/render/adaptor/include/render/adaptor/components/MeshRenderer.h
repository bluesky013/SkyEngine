//
// Created by Zach Lee on 2023/2/28.
//

#pragma once

#include <framework/world/Component.h>
#include <render/adaptor/assets/MeshAsset.h>
#include <render/resource/Mesh.h>
#include <render/mesh/StaticMeshRenderer.h>

namespace sky {

    class MeshRenderer : public ComponentBase {
    public:
        MeshRenderer() = default;
        ~MeshRenderer() override;

        COMPONENT_RUNTIME_INFO(MeshRenderer)

        static void Reflect(SerializationContext *context);

        void OnActive() override;
        void OnDeActive() override;
        void Tick(float time) override;

        void SetMesh(const MeshAssetPtr &mesh);
        void SetMesh(const RDMeshPtr &mesh);

        void SaveJson(JsonOutputArchive &ar) const override;
        void LoadJson(JsonInputArchive &ar) override;

        StaticMeshRenderer *GetRenderer() const { return renderer; }

    private:
        void ResetMesh();
        void ShutDown();

        bool isStatic = true;
        bool castShadow = false;
        bool receiveShadow = false;

        MeshAssetPtr meshAsset;
        RDMeshPtr meshInstance;
        StaticMeshRenderer *renderer = nullptr;
    };

} // namespace receiveShadow
