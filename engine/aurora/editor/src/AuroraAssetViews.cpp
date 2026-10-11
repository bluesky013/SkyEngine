//
// Aurora editor asset-view providers (see AuroraAssetViews.h).
//

#include <aurora/editor/AuroraAssetViews.h>

#include <aurora/editor/MaterialEditorView.h>

#include <aurora/adaptor/assets/ImageAsset.h>
#include <aurora/adaptor/assets/LodGroupAsset.h>
#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/adaptor/assets/MaterialInstanceAsset.h>
#include <aurora/adaptor/assets/MeshAsset.h>
#include <aurora/adaptor/assets/SkeletonAsset.h>

#include <core/environment/Once.h>
#include <core/math/Vector3.h>
#include <editor/core/asset/AssetViewData.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/SerializationContext.h>

#include <string>
#include <utility>

namespace sky::aurora::editor {

    namespace {

        // ---- reflected view structs (read-only MVP) ----

        struct ImageView {
            uint32_t width       = 0;
            uint32_t height      = 0;
            uint32_t depth       = 0;
            uint32_t mipLevels   = 0;
            uint32_t arrayLayers = 0;
            uint32_t format      = 0;
            uint32_t imageType   = 0;
            uint32_t dataSize    = 0;
            uint32_t sliceCount  = 0;
        };

        struct MeshView {
            uint32_t vertexCount   = 0;
            uint32_t vertexStride  = 0;
            uint32_t indexCount    = 0;
            uint32_t indexType     = 0;
            uint32_t subMeshCount  = 0;
            uint32_t materialCount = 0;
            uint32_t skinBoneCount = 0;
            Vector3  boundsCenter;
            Vector3  boundsExtent;
            float    boundsRadius = 0.0f;
        };

        struct SkeletonView {
            uint32_t boneCount = 0;
        };

        struct LodGroupView {
            uint32_t levelCount = 0;
        };

        // ---- providers ----

        class ImageViewProvider : public sky::editor::IAssetViewDataProvider {
        public:
            sky::editor::AssetViewData Build(const Uuid &uuid, const std::string &) override
            {
                auto asset = AssetManager::Get()->LoadAsset<Texture>(uuid);
                if (asset == nullptr) {
                    return {};
                }
                asset->BlockUntilLoaded();
                const auto &data = asset->Data();

                ImageView view;
                view.width       = data.width;
                view.height      = data.height;
                view.depth       = data.depth;
                view.mipLevels   = data.mipLevels;
                view.arrayLayers = data.arrayLayers;
                view.format      = static_cast<uint32_t>(data.format);
                view.imageType   = static_cast<uint32_t>(data.type);
                view.dataSize    = static_cast<uint32_t>(data.dataSize);
                view.sliceCount  = static_cast<uint32_t>(data.slices.size());

                sky::Any any(view);
                auto     node = sky::GetTypeNode(any);
                return sky::editor::AssetViewData{std::move(any), node};
            }
        };

        class MeshViewProvider : public sky::editor::IAssetViewDataProvider {
        public:
            sky::editor::AssetViewData Build(const Uuid &uuid, const std::string &) override
            {
                auto asset = AssetManager::Get()->LoadAsset<Mesh>(uuid);
                if (asset == nullptr) {
                    return {};
                }
                asset->BlockUntilLoaded();
                const auto &data = asset->Data();

                MeshView view;
                view.vertexCount   = data.vertexCount;
                view.vertexStride  = data.vertexStride;
                view.indexCount    = data.indexCount;
                view.indexType     = data.indexType;
                view.subMeshCount  = static_cast<uint32_t>(data.subMeshes.size());
                view.materialCount = static_cast<uint32_t>(data.materials.size());
                view.skinBoneCount = data.skinBoneCount;
                view.boundsCenter  = data.bounds.center;
                view.boundsExtent  = data.bounds.extent;
                view.boundsRadius  = data.bounds.radius;

                sky::Any any(view);
                auto     node = sky::GetTypeNode(any);
                return sky::editor::AssetViewData{std::move(any), node};
            }
        };

        class SkeletonViewProvider : public sky::editor::IAssetViewDataProvider {
        public:
            sky::editor::AssetViewData Build(const Uuid &uuid, const std::string &) override
            {
                auto asset = AssetManager::Get()->LoadAsset<Skeleton>(uuid);
                if (asset == nullptr) {
                    return {};
                }
                asset->BlockUntilLoaded();

                SkeletonView view;
                view.boneCount = static_cast<uint32_t>(asset->Data().boneData.size());

                sky::Any any(view);
                auto     node = sky::GetTypeNode(any);
                return sky::editor::AssetViewData{std::move(any), node};
            }
        };

        class LodGroupViewProvider : public sky::editor::IAssetViewDataProvider {
        public:
            sky::editor::AssetViewData Build(const Uuid &uuid, const std::string &) override
            {
                auto asset = AssetManager::Get()->LoadAsset<LodGroup>(uuid);
                if (asset == nullptr) {
                    return {};
                }
                asset->BlockUntilLoaded();

                LodGroupView view;
                view.levelCount = static_cast<uint32_t>(asset->Data().levels.size());

                sky::Any any(view);
                auto     node = sky::GetTypeNode(any);
                return sky::editor::AssetViewData{std::move(any), node};
            }
        };

        ImageViewProvider    g_imageView;
        MeshViewProvider     g_meshView;
        SkeletonViewProvider g_skeletonView;
        LodGroupViewProvider g_lodGroupView;

    } // namespace

    void RegisterAuroraAssetViews(sky::SerializationContext *context)
    {
        if (context == nullptr || !sky::Once::Get()->Run("AuroraAssetViews")) {
            return; // cross-DLL idempotency
        }

        // Read-only MVP: these are asset summaries; editable import/cook settings live in the
        // BUNDLE section. Source write-back (edit -> re-cook) is a follow-up.
        constexpr uint32_t kReadOnly = static_cast<uint32_t>(CommonPropertyKey::READONLY);

        context->Register<ImageView>("ImageView")
            .Member<&ImageView::width>("width")
            .Property(kReadOnly, Any(true))
            .Member<&ImageView::height>("height")
            .Property(kReadOnly, Any(true))
            .Member<&ImageView::depth>("depth")
            .Property(kReadOnly, Any(true))
            .Member<&ImageView::mipLevels>("mipLevels")
            .Property(kReadOnly, Any(true))
            .Member<&ImageView::arrayLayers>("arrayLayers")
            .Property(kReadOnly, Any(true))
            .Member<&ImageView::format>("format")
            .Property(kReadOnly, Any(true))
            .Member<&ImageView::imageType>("imageType")
            .Property(kReadOnly, Any(true))
            .Member<&ImageView::dataSize>("dataSize")
            .Property(kReadOnly, Any(true))
            .Member<&ImageView::sliceCount>("sliceCount")
            .Property(kReadOnly, Any(true));

        context->Register<MeshView>("MeshView")
            .Member<&MeshView::vertexCount>("vertexCount")
            .Property(kReadOnly, Any(true))
            .Member<&MeshView::vertexStride>("vertexStride")
            .Property(kReadOnly, Any(true))
            .Member<&MeshView::indexCount>("indexCount")
            .Property(kReadOnly, Any(true))
            .Member<&MeshView::indexType>("indexType")
            .Property(kReadOnly, Any(true))
            .Member<&MeshView::subMeshCount>("subMeshCount")
            .Property(kReadOnly, Any(true))
            .Member<&MeshView::materialCount>("materialCount")
            .Property(kReadOnly, Any(true))
            .Member<&MeshView::skinBoneCount>("skinBoneCount")
            .Property(kReadOnly, Any(true))
            .Member<&MeshView::boundsCenter>("boundsCenter")
            .Property(kReadOnly, Any(true))
            .Member<&MeshView::boundsExtent>("boundsExtent")
            .Property(kReadOnly, Any(true))
            .Member<&MeshView::boundsRadius>("boundsRadius")
            .Property(kReadOnly, Any(true));

        context->Register<SkeletonView>("SkeletonView").Member<&SkeletonView::boneCount>("boneCount").Property(kReadOnly, Any(true));

        context->Register<LodGroupView>("LodGroupView").Member<&LodGroupView::levelCount>("levelCount").Property(kReadOnly, Any(true));

        auto *registry = sky::editor::AssetViewDataRegistry::Get();
        registry->Register(std::string(AssetTraits<Texture>::ASSET_TYPE), &g_imageView);
        registry->Register(std::string(AssetTraits<Mesh>::ASSET_TYPE), &g_meshView);
        registry->Register(std::string(AssetTraits<Skeleton>::ASSET_TYPE), &g_skeletonView);
        registry->Register(std::string(AssetTraits<LodGroup>::ASSET_TYPE), &g_lodGroupView);

        // Material / MaterialInstance are heterogeneous (typed property lists) -> custom editor providers.
        RegisterMaterialEditorProvider();
    }

} // namespace sky::aurora::editor
