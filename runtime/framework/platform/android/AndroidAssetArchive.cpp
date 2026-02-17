//
// Created by blues on 2024/5/16.
//

#include "AndroidAssetArchive.h"
#include <android/asset_manager.h>
#include <framework/platform/PlatformBase.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

namespace sky {

    AndroidAssetArchive::~AndroidAssetArchive()
    {
        AAsset_close(assetFile);
    }

    AndroidAssetArchive::AndroidAssetArchive(AAsset *asset)
        : IStreamArchive(ss)
        , assetFile(asset)
    {
        auto length = AAsset_getLength(assetFile);
        const auto *ptr = AAsset_getBuffer(assetFile);
        ss.write(reinterpret_cast<const char*>(ptr), length);
    }

    bool AndroidAssetArchive::LoadRaw(char *out, size_t size)
    {
        return AAsset_read(assetFile, out, size) == size;
    }

} // namespace sky