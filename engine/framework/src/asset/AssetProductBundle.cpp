#include <framework/asset/AssetProductBundle.h>

namespace sky {
    FilePtr HashedAssetBundle::OpenFile(const Uuid &uuid) const
    {
        return uuid ? fs->OpenFile(ProductFilePath(uuid)) : FilePtr{};
    }

    FilePtr HashedAssetBundle::CreateOrOpenFile(const Uuid &uuid) const
    {
        return uuid ? fs->CreateOrOpenFile(ProductFilePath(uuid)) : FilePtr{};
    }

    FilePtr PackageAssetBundle::OpenFile(const Uuid &uuid) const
    {
        return uuid ? fs->OpenFile(ProductFilePath(uuid)) : FilePtr{};
    }
} // namespace sky
