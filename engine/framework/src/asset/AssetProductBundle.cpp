//
// Created by blues on 2024/6/26.
//

#include <framework/asset/AssetProductBundle.h>

namespace sky {

    static FilePtr OpenBundleProduct(const FileSystemPtr &fs, const Uuid &uuid)
    {
        const AssetProductGroupStore groups(fs);
        if (groups.HasOwnerRecord(uuid)) {
            ProductGroupSnapshotPtr snapshot;
            FilePtr                 file;
            if (groups.Resolve(uuid, snapshot) && groups.Open(*snapshot, uuid, file)) {
                return file;
            }
            if (!groups.HasLegacyFallback(uuid)) {
                return {};
            }
        }
        auto strUuid = uuid.ToString();
        auto sub     = strUuid.substr(0, 2);
        return fs->OpenFile(sub + "/" + strUuid + ".bin");
    }

    FilePtr HashedAssetBundle::OpenFile(const Uuid &uuid) const
    {
        return OpenBundleProduct(fs, uuid);
    }

    FilePtr PackageAssetBundle::OpenFile(const Uuid &uuid) const
    {
        return OpenBundleProduct(fs, uuid);
    }

    FilePtr HashedAssetBundle::CreateOrOpenFile(const Uuid &uuid) const
    {
        if (AssetProductGroupStore(fs).HasOwnerRecord(uuid)) {
            return {};
        }
        auto strUuid = uuid.ToString();
        auto sub     = strUuid.substr(0, 2);
        auto subFs   = static_cast<NativeFileSystem *>(fs.Get())->CreateSubSystem(sub, true);
        return subFs->CreateOrOpenFile(strUuid + ".bin");
    }
} // namespace sky
