//
// Created by blues on 2024/6/26.
//

#pragma once

#include <core/file/FileSystem.h>
#include <core/util/Uuid.h>

namespace sky {

    using ProductBundleKey = std::string;
    class AssetProductBundle {
    public:
        explicit AssetProductBundle(const FileSystemPtr &fs_, const ProductBundleKey &key_) // NOLINT
            : fs(fs_), key(key_)
        {
        }
        virtual ~AssetProductBundle() = default;

        virtual FilePtr OpenFile(const Uuid &uuid) const = 0;
        virtual FilePtr CreateOrOpenFile(const Uuid &uuid) const = 0;

        const ProductBundleKey &GetKey() const { return key; }
        virtual bool IsPacked() const { return false; }

    protected:
        FileSystemPtr fs;
        ProductBundleKey key;
    };

    class HashedAssetBundle : public AssetProductBundle {
    public:
        explicit HashedAssetBundle(const NativeFileSystemPtr &fs, const ProductBundleKey &key) : AssetProductBundle(fs, key) {}
        ~HashedAssetBundle() override = default;

    protected:
        FilePtr OpenFile(const Uuid &uuid) const override;
        FilePtr CreateOrOpenFile(const Uuid &uuid) const override;
    };

    class PackageAssetBundle : public AssetProductBundle {
    public:
        explicit PackageAssetBundle(const FileSystemPtr &fs, const ProductBundleKey &key) : AssetProductBundle(fs, key) {}
        ~PackageAssetBundle() override = default;

        bool IsPacked() const override { return true; }
    protected:
        FilePtr OpenFile(const Uuid &uuid) const override { return {}; }
        FilePtr CreateOrOpenFile(const Uuid &uuid) const override { return {}; }
    };
} // namespace sky