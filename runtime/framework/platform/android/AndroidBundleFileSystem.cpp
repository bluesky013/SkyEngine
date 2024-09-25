//
// Created by blues on 2024/5/16.
//

#include "AndroidBundleFileSystem.h"
#include "AndroidAssetArchive.h"

#include <framework/platform/PlatformBase.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

namespace sky {

    bool AndroidBundleFileSystem::FileExist(const std::string &path)
    {
        auto *am = static_cast<android_app*>(Platform::Get()->GetNativeApp())->activity->assetManager;
        AAsset* assetFile = AAssetManager_open(am, path.c_str(), AASSET_MODE_UNKNOWN);
        if (assetFile == nullptr) {
            return false;
        }
        AAsset_close(assetFile);
        return true;
    }

    IArchivePtr AndroidBundleFileSystem::ReadAsArchive(const std::string &path)
    {
        auto *am = static_cast<android_app*>(Platform::Get()->GetNativeApp())->activity->assetManager;
        AAsset* assetFile = AAssetManager_open(am, path.c_str(), AASSET_MODE_BUFFER);
        if (assetFile != nullptr) {
            return std::make_shared<AndroidAssetArchive>(assetFile);
        }
        return {};
    }

    OArchivePtr AndroidBundleFileSystem::WriteAsArchive(const std::string &path)
    {
        return {};
    }

    FilePtr AndroidBundleFileSystem::OpenFile(const std::string &name)
    {
        return std::make_shared<AndroidAssetFile>(name);
    }

    bool AndroidBundleFileSystem::ReadString(const std::string &path, std::string &out)
    {
        auto *am = static_cast<android_app*>(Platform::Get()->GetNativeApp())->activity->assetManager;
        AAsset* assetFile = AAssetManager_open(am, path.c_str(), AASSET_MODE_BUFFER);
        if (assetFile == nullptr) {
            return false;
        }

        size_t assetLength = AAsset_getLength(assetFile);
        out.resize(assetLength);
        AAsset_read(assetFile, out.data(), assetLength);
        AAsset_close(assetFile);
        return true;
    }

    void AndroidAssetFile::ReadData(uint64_t offset, uint64_t size, uint8_t *out)
    {
        auto *am = static_cast<android_app*>(Platform::Get()->GetNativeApp())->activity->assetManager;
        AAsset* assetFile = AAssetManager_open(am, path.c_str(), AASSET_MODE_RANDOM);
        if (assetFile == nullptr) {
            return;
        }

        size_t assetLength = AAsset_getLength(assetFile);
        if (offset + size <= assetLength) {
            AAsset_seek64(assetFile, offset, SEEK_SET);
            AAsset_read(assetFile, out, size);
        }
        AAsset_close(assetFile);
    }

} // namespace sky