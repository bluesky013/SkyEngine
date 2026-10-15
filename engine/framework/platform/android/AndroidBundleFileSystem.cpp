//
// Created by blues on 2024/5/16.
//

#include "AndroidBundleFileSystem.h"
#include "AndroidAssetArchive.h"
#include "AndroidAssetUtils.h"

#include <android/asset_manager.h>
#include <core/logger/Logger.h>
#include <framework/platform/PlatformBase.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <limits>
#include <memory>

static const char *TAG = "AndroidAsset";

namespace sky {

    namespace {
        struct AssetCloser {
            void operator()(AAsset *asset) const
            {
                if (asset != nullptr) {
                    AAsset_close(asset);
                }
            }
        };
        using AssetHandle = std::unique_ptr<AAsset, AssetCloser>;

        AssetHandle OpenAsset(const std::string &path, int mode)
        {
            auto       *platform = Platform::Get()->GetImpl();
            const auto *app      = platform != nullptr ? static_cast<android_app *>(platform->GetNativeApp()) : nullptr;
            if (app == nullptr || app->activity == nullptr || app->activity->assetManager == nullptr || path.empty()) {
                return {};
            }
            return AssetHandle(AAssetManager_open(app->activity->assetManager, path.c_str(), mode));
        }

        bool ReadAsset(AAsset *asset, char *out, size_t size)
        {
            return ReadAndroidAssetBytes([asset](char *buffer, size_t count) { return AAsset_read(asset, buffer, count); }, out, size);
        }
    } // namespace

    AndroidBundleFileSystem::AndroidBundleFileSystem(const std::string &path)
    {
        valid = NormalizeAndroidAssetPath(path, basePath);
        if (valid && !basePath.empty()) {
            basePath += '/';
        }
    }

    bool AndroidBundleFileSystem::FileExist(const FilePath &path) const
    {
        std::string normalized;
        if (!valid || !NormalizeAndroidAssetPath(basePath + path.GetStr(), normalized)) {
            return false;
        }
        return static_cast<bool>(OpenAsset(normalized, AASSET_MODE_UNKNOWN));
    }

    FilePtr AndroidBundleFileSystem::OpenFile(const FilePath &name)
    {
        std::string normalized;
        if (!valid || !NormalizeAndroidAssetPath(basePath + name.GetStr(), normalized) || !OpenAsset(normalized, AASSET_MODE_UNKNOWN)) {
            return {};
        }
        return new AndroidAssetFile(normalized);
    }

    FilePtr AndroidBundleFileSystem::CreateOrOpenFile(const FilePath &name)
    {
        (void)name;
        return {}; // Packaged APK assets are read-only.
    }

    FileSystemPtr AndroidBundleFileSystem::CreateSubSystem(const std::string &path, bool createDir)
    {
        if (!valid || createDir) {
            return {};
        }
        std::string normalized;
        if (!NormalizeAndroidAssetPath(basePath + path, normalized)) {
            return {};
        }
        return new AndroidBundleFileSystem(normalized);
    }

    AndroidAssetFile::AndroidAssetFile(const FilePath &name)
    {
        valid = NormalizeAndroidAssetPath(name.GetStr(), path);
    }

    void AndroidAssetFile::ReadData(uint64_t offset, uint64_t size, uint8_t *out)
    {
        const auto asset = valid ? OpenAsset(path, AASSET_MODE_RANDOM) : AssetHandle{};
        if (!asset || (size != 0 && out == nullptr) || size > std::numeric_limits<size_t>::max()) {
            return;
        }
        const auto length = AAsset_getLength64(asset.get());
        if (length < 0 || offset > static_cast<uint64_t>(length) || size > static_cast<uint64_t>(length) - offset ||
            AAsset_seek64(asset.get(), static_cast<off64_t>(offset), SEEK_SET) < 0) {
            return;
        }
        if (!ReadAsset(asset.get(), reinterpret_cast<char *>(out), static_cast<size_t>(size))) {
            LOG_E(TAG, "Short read from APK asset '%s'", path.c_str());
        }
    }

    IStreamArchivePtr AndroidAssetFile::ReadAsArchive()
    {
        auto asset = valid ? OpenAsset(path, AASSET_MODE_STREAMING) : AssetHandle{};
        if (!asset) {
            return {};
        }
        const CounterPtr<AndroidAssetArchive> archive = new AndroidAssetArchive(asset.release());
        return archive->IsValid() ? IStreamArchivePtr(archive) : IStreamArchivePtr{};
    }

    OStreamArchivePtr AndroidAssetFile::WriteAsArchive()
    {
        return {};
    }

    bool AndroidAssetFile::ReadBin(std::vector<uint8_t> &out)
    {
        out.clear();
        const auto asset = valid ? OpenAsset(path, AASSET_MODE_STREAMING) : AssetHandle{};
        if (!asset) {
            return false;
        }
        const auto length = AAsset_getLength64(asset.get());
        if (length < 0 || static_cast<uint64_t>(length) > out.max_size()) {
            return false;
        }
        out.resize(static_cast<size_t>(length));
        if (!ReadAsset(asset.get(), reinterpret_cast<char *>(out.data()), out.size())) {
            out.clear();
            return false;
        }
        return true;
    }

    BinaryDataPtr AndroidAssetFile::ReadBin()
    {
        const auto asset = valid ? OpenAsset(path, AASSET_MODE_STREAMING) : AssetHandle{};
        if (!asset) {
            return {};
        }
        const auto length = AAsset_getLength64(asset.get());
        if (length < 0 || static_cast<uint64_t>(length) > std::numeric_limits<uint32_t>::max()) {
            return {};
        }
        const BinaryDataPtr data = new BinaryData(static_cast<uint32_t>(length));
        return ReadAsset(asset.get(), reinterpret_cast<char *>(data->Data()), data->Size()) ? data : BinaryDataPtr{};
    }

    bool AndroidAssetFile::ReadString(std::string &out)
    {
        out.clear();
        const auto asset = valid ? OpenAsset(path, AASSET_MODE_STREAMING) : AssetHandle{};
        if (!asset) {
            return false;
        }
        const auto length = AAsset_getLength64(asset.get());
        if (length < 0 || static_cast<uint64_t>(length) > out.max_size()) {
            return false;
        }
        out.resize(static_cast<size_t>(length));
        if (!ReadAsset(asset.get(), out.data(), out.size())) {
            out.clear();
            return false;
        }
        return true;
    }

} // namespace sky
