//
// Created by blues on 2024/7/23.
//

#pragma once

#include <core/event/Event.h>
#include <framework/asset/Asset.h>

namespace sky {

    struct AssetBuildResult;

    class IAssetEvent : public EventTraits {
    public:
        IAssetEvent() = default;
        virtual ~IAssetEvent() = default;

        using KeyType = Uuid;
        using MutexType = std::mutex;

        virtual void OnAssetBuildFinished(const AssetBuildResult &result) {}
        virtual void OnAssetLoaded() {}
        virtual void OnAssetReload() {}
        virtual void OnAssetReset() {}
    };
    using AsseEvent = Event<IAssetEvent>;

} // namespace sky
