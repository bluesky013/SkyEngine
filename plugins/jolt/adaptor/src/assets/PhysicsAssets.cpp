#include "PhysicsAssets.h"

#include <framework/asset/AssetManager.h>

namespace sky::phy {
    namespace {
        template <typename T, auto Parse, auto Write>
        class PhysicsAssetHandler final : public AssetHandlerBase {
        public:
            AssetPtr CreateAsset() override
            {
                return std::make_shared<Asset<T>>();
            }
            bool Load(IStreamArchive &archive, const AssetPtr &asset) override
            {
                std::string text;
                return ReadPhysicsText(archive.GetStream(), text) &&
                       static_cast<bool>(Parse(text, std::static_pointer_cast<Asset<T>>(asset)->Data()));
            }
            void Save(OStreamArchive &archive, const AssetPtr &asset) override
            {
                SaveChecked(archive, asset);
            }
            bool SaveChecked(OStreamArchive &archive, const AssetPtr &asset) override
            {
                std::string text;
                if (!Write(std::static_pointer_cast<Asset<T>>(asset)->Data(), text))
                    return false;
                archive.GetStream().write(text.data(), text.size());
                return archive.GetStream().good();
            }
        };
    } // namespace
    void RegisterPhysicsAssets()
    {
        auto *manager = AssetManager::Get();
        manager->RegisterAssetHandler(COLLISION_ASSET_TYPE, new PhysicsAssetHandler<CollisionAsset, ParseCollision, WriteCollision>());
        manager->RegisterAssetHandler(MATERIAL_ASSET_TYPE, new PhysicsAssetHandler<MaterialAsset, ParsePhysicsMaterial, WritePhysicsMaterial>());
    }
} // namespace sky::phy
