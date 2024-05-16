//
// Created by blues on 2024/5/16.
//

#pragma once

#include <core/archive/IArchive.h>
#include <core/archive/MemoryArchive.h>
#include <string>
#include <android/asset_manager.h>

namespace sky {

    class AndroidAssetArchive : public IInputArchive {
    public:
        explicit AndroidAssetArchive(AAsset *asset);
        ~AndroidAssetArchive() override;

        bool LoadRaw(char *data, size_t size) override;
    private:
        AAsset *assetFile;
    };

}