//
// Created by blues on 2024/5/16.
//

#include "AndroidAssetArchive.h"
#include "AndroidAssetUtils.h"
#include <android/asset_manager.h>
#include <limits>

namespace sky {

    AndroidAssetArchive::~AndroidAssetArchive()
    {
        if (assetFile != nullptr) {
            AAsset_close(assetFile);
        }
    }

    AndroidAssetArchive::AndroidAssetArchive(AAsset *asset) : IStreamArchive(ss), assetFile(asset)
    {
        if (assetFile == nullptr) {
            return;
        }
        const auto  length = AAsset_getLength64(assetFile);
        std::string bytes;
        if (length < 0 || static_cast<uint64_t>(length) > bytes.max_size()) {
            return;
        }
        bytes.resize(static_cast<size_t>(length));
        if (!ReadAndroidAssetBytes([this](char *data, size_t size) { return AAsset_read(assetFile, data, size); }, bytes.data(), bytes.size())) {
            return;
        }
        ss.str(std::move(bytes));
        ss.seekg(0);
        valid = true;
    }

    bool AndroidAssetArchive::IsValid() const
    {
        return valid;
    }

} // namespace sky
