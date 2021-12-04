//
// Created by Zach Lee on 2021/12/3.
//

#pragma once

namespace sky {

    class AssetManager {
    public:
        static AssetManager* Get();
        static AssetManager* Destroy();

    private:
        AssetManager() = default;
        ~AssetManager() = default;
    };

}