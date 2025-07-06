//
// Created by blues on 2025/7/15.
//

#pragma once

#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetEvent.h>

namespace sky {

    template <typename T>
    class SingleAssetHolder {
    public:
        SingleAssetHolder() = default;
        ~SingleAssetHolder() = default;

        void SetAsset(const Uuid& uuid, IAssetEvent* listener)
        {
            const auto &current = asset ? asset->GetUuid() : Uuid::GetEmpty();
            if (current == uuid) {
                return;
            }

            if (uuid) {
                binder.Bind(listener, uuid);
            } else {
                binder.Reset();
            }

            asset = uuid ? AssetManager::Get()->LoadAsset<T>(uuid) : std::shared_ptr<Asset<T>> {};
            if (!asset) {
                listener->OnAssetReset();
            } else if (asset->IsLoaded()) {
                listener->OnAssetLoaded();
            }
        }

        bool IsLoaded() const
        {
            return asset && asset->IsLoaded();
        }

        Asset<T>::DataType &Data() { return asset->Data(); }

        const std::shared_ptr<Asset<T>> &GetAsset() const { return asset; }
    private:
        std::shared_ptr<Asset<T>> asset;
        EventBinder<IAssetEvent, Uuid> binder;
    };

} // namespace sky
