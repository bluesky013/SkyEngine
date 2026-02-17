//
// Created by bluesky on 2023/4/15.
//

//#include <framework/asset/AssetStream.h>
#include <android/asset_manager.h>
#include <framework/platform/PlatformBase.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

namespace sky {

//    std::string AssetStream::ReadString() {
//        auto *am = static_cast<android_app*>(Platform::Get()->GetNativeApp())->activity->assetManager;
//        AAsset* asset = AAssetManager_open(am, path.c_str(), AASSET_MODE_BUFFER);
//
//        std::string res;
//        size_t assetLength = AAsset_getLength(asset);
//        res.resize(assetLength);
//        AAsset_read(asset, res.data(), assetLength);
//        return res;
//    }
} // namespace sky