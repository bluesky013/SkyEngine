//
// Created by blues on 2024/6/26.
//

#include <framework/asset/AssetProductBundle.h>

namespace sky {

    FilePtr HashedAssetBundle::OpenFile(const Uuid &uuid) const
    {
        auto strUuid = uuid.ToString();
        auto sub = strUuid.substr(0, 2);
        auto subFs = static_cast<NativeFileSystem *>(fs.Get())->CreateSubSystem(sub, false);
        return subFs->OpenFile(strUuid + ".bin");
    }

    FilePtr HashedAssetBundle::CreateOrOpenFile(const Uuid &uuid) const
    {
        auto strUuid = uuid.ToString();
        auto sub = strUuid.substr(0, 2);
        auto subFs = static_cast<NativeFileSystem *>(fs.Get())->CreateSubSystem(sub, true);
        return subFs->CreateOrOpenFile(strUuid + ".bin");
    }
} // namespace sky