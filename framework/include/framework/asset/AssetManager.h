//
// Created by yjrj on 2022/8/8.
//

#pragma once

#include <core/environment/Singleton.h>
#include <framework/asset/Asset.h>

namespace sky {

    class AssetManager : public Singleton<AssetManager> {
    public:
        ~AssetManager() = default;

    private:
        friend class Singleton<AssetManager>;
        AssetManager() = default;
    };

}
