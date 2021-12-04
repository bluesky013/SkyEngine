//
// Created by Zach Lee on 2021/12/3.
//

#pragma once

#include <framework/environment/Singleton.h>

namespace sky {

    class AssetManager : public Singleton<AssetManager> {
    public:

    protected:
        friend class Singleton<AssetManager>;
        AssetManager();
        ~AssetManager();
    };

}