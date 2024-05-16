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

    AndroidAssetArchive::AndroidAssetArchive(AAsset *asset) : assetFile(asset)
    {
    }

    bool AndroidAssetArchive::LoadRaw(char *out, size_t size)
    {
        return AAsset_read(assetFile, out, size) == size;
    }

} // namespace sky