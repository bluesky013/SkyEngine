//
// Content editor asset-view providers (see ContentAssetViews.h).
//

#include <content/editor/ContentAssetViews.h>

#include <content/adaptor/assets/AnimationAsset.h>

#include <editor/core/asset/AssetViewData.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/SerializationContext.h>

#include <string>
#include <utility>

namespace sky::content::editor {

    namespace {

        struct AnimationClipView {
            uint32_t channelCount  = 0;
            float    frameRate     = 0.0f;
            uint32_t skeletonBound = 0;
        };


        class ClipViewProvider : public sky::editor::IAssetViewDataProvider {
        public:
            sky::editor::AssetViewData Build(const Uuid &uuid, const std::string &) override
            {
                auto asset = AssetManager::Get()->LoadAsset<AnimationClip>(uuid);
                if (asset == nullptr) {
                    return {};
                }
                asset->BlockUntilLoaded();
                const auto &data = asset->Data();

                AnimationClipView view;
                view.channelCount  = static_cast<uint32_t>(data.channels.size());
                view.frameRate     = data.frameRate;
                view.skeletonBound = data.skeleton ? 1u : 0u;

                sky::Any any(view);
                auto     node = sky::GetTypeNode(any);
                return sky::editor::AssetViewData{std::move(any), node};
            }
        };

        ClipViewProvider  g_clipView;

    } // namespace

    void ReflectContentAssetViews(sky::SerializationContext *context)
    {

        // Read-only MVP: asset summaries; source write-back (edit -> re-cook) is a follow-up.
        constexpr uint32_t kReadOnly = static_cast<uint32_t>(CommonPropertyKey::READONLY);

        context->Register<AnimationClipView>("AnimationClipView")
            .Member<&AnimationClipView::channelCount>("channelCount")
            .Property(kReadOnly, Any(true))
            .Member<&AnimationClipView::frameRate>("frameRate")
            .Property(kReadOnly, Any(true))
            .Member<&AnimationClipView::skeletonBound>("skeletonBound")
            .Property(kReadOnly, Any(true));
    }

    void RegisterContentAssetViews(std::shared_ptr<const void> lease)
    {
        auto *registry = sky::editor::AssetViewDataRegistry::Get();
        registry->Register(std::string(AssetTraits<AnimationClip>::ASSET_TYPE), &g_clipView, lease);
    }

    void UnregisterContentAssetViews()
    {
        auto *registry = sky::editor::AssetViewDataRegistry::Get();
        registry->Unregister(std::string(AssetTraits<AnimationClip>::ASSET_TYPE), &g_clipView);
    }

} // namespace sky::content::editor
