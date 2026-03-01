//
// Created by blues on 2025/7/15.
//

#pragma once

#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetEvent.h>

namespace sky {

    struct IAssetReadyNotifier {
        virtual ~IAssetReadyNotifier() = default;

        virtual void OnAssetLoaded(const Uuid& uuid, const std::string_view& type) {}
    };

    template <typename T>
    class SingleAssetHolder : public IAssetEvent {
    public:
        SingleAssetHolder() = default;
        ~SingleAssetHolder() override = default;

        void SetAsset(const Uuid& uuid, IAssetReadyNotifier* inListener)
        {
            const auto &current = asset ? asset->GetUuid() : Uuid::GetEmpty();
            if (current == uuid) {
                return;
            }

            listener = inListener;

            if (uuid) {
                binder.Bind(this, uuid);
            } else {
                binder.Reset();
            }

            asset = uuid ? AssetManager::Get()->LoadAsset<T>(uuid) : std::shared_ptr<Asset<T>> {};
            if (asset && asset->IsLoaded()) {
                listener->OnAssetLoaded(asset->GetUuid(), AssetTraits<T>::ASSET_TYPE);
            }
        }

        bool IsLoaded() const
        {
            return asset && asset->IsLoaded();
        }

        const Uuid& GetUuid() const
        {
            return asset ? asset->GetUuid() : Uuid::GetEmpty();
        }

        Asset<T>::DataType &Data() { return asset->Data(); }

        const std::shared_ptr<Asset<T>> &GetAsset() const { return asset; }
    private:
        void OnAssetLoaded() override
        {
            listener->OnAssetLoaded(asset->GetUuid(), AssetTraits<T>::ASSET_TYPE);
        }

        IAssetReadyNotifier* listener = nullptr;
        std::shared_ptr<Asset<T>> asset;
        EventBinder<IAssetEvent, Uuid> binder;
    };

} // namespace sky
