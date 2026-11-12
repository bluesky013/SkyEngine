#pragma once

#include <editor/core/asset/AssetPreviewProvider.h>
#include <string_view>

namespace sky::aurora::editor {
    std::string ImportedMeshSourceKey(const Uuid &owner, const Uuid &mesh);
    Uuid SelectedMeshProduct(const Uuid &openedMesh, std::string_view openedSourceKey, const Uuid &sourceOwner, std::string_view selectedSourceKey);

    class MeshMaterialPreview {
    public:
        MeshMaterialPreview() = default;
        ~MeshMaterialPreview();
        MeshMaterialPreview(const MeshMaterialPreview &)            = delete;
        MeshMaterialPreview &operator=(const MeshMaterialPreview &) = delete;

        const sky::editor::AssetPreviewStatus &Update(const Uuid                      &mesh,
                                                      const sky::editor::EditorCamera &camera,
                                                      uint32_t                         width,
                                                      uint32_t                         height,
                                                      std::string_view                 sourceKey,
                                                      const std::optional<AABB>       &subjectBounds);
        void                                   Close();

    private:
        sky::editor::IAssetPreviewProvider *owner = nullptr;
        sky::editor::AssetPreviewHandle     session;
        sky::editor::AssetPreviewStatus     status;
        Uuid                                product;
        std::string                         sourceKey;
        uint32_t                            width = 0, height = 0;
    };
} // namespace sky::aurora::editor
