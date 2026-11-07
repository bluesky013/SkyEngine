#include <particle/editor/ParticleEditor.h>

#include <editor/core/asset/AssetViewData.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/serialization/SerializationContext.h>
#include <particle/assets/ParticleEffectAsset.h>

namespace sky::particle::editor {
    namespace {
        struct ParticleEffectView {
            uint32_t emitterCount  = 0;
            uint32_t programCount  = 0;
            uint32_t totalCapacity = 0;
            uint64_t sourceHash    = 0;
        };

        class ParticleEffectViewProvider final : public sky::editor::IAssetViewDataProvider {
        public:
            sky::editor::AssetViewData Build(const Uuid &uuid, const std::string &) override
            {
                auto asset = AssetManager::Get()->LoadAsset<ParticleEffectAsset>(uuid);
                if (!asset)
                    return {};
                asset->BlockUntilLoaded();
                if (!asset->IsLoaded() || !asset->Data().IsLoadValid())
                    return {};
                ParticleEffectView view;
                view.programCount = static_cast<uint32_t>(asset->Data().programs.size());
                view.emitterCount = view.programCount;
                view.sourceHash   = asset->Data().sourceHash;
                for (const auto &program : asset->Data().programs)
                    view.totalCapacity += program.capacity;
                sky::Any any(view);
                auto     node = sky::GetTypeNode(any);
                return sky::editor::AssetViewData{std::move(any), node};
            }
        };

        ParticleEffectViewProvider g_particleView;
    } // namespace

    void RegisterParticleAssetViews(SerializationContext *context)
    {
        constexpr uint32_t readOnly = static_cast<uint32_t>(CommonPropertyKey::READONLY);
        context->Register<ParticleEffectView>("ParticleEffectView")
            .Member<&ParticleEffectView::emitterCount>("emitterCount")
            .Property(readOnly, Any(true))
            .Member<&ParticleEffectView::programCount>("programCount")
            .Property(readOnly, Any(true))
            .Member<&ParticleEffectView::totalCapacity>("totalCapacity")
            .Property(readOnly, Any(true))
            .Member<&ParticleEffectView::sourceHash>("sourceHash")
            .Property(readOnly, Any(true));
    }
    void RegisterParticleAssetViews()
    {
        sky::editor::AssetViewDataRegistry::Get()->Register(std::string(AssetTraits<ParticleEffectAsset>::ASSET_TYPE), &g_particleView);
    }
    void UnregisterParticleAssetViews()
    {
        sky::editor::AssetViewDataRegistry::Get()->Unregister(std::string(AssetTraits<ParticleEffectAsset>::ASSET_TYPE), &g_particleView);
    }
} // namespace sky::particle::editor
