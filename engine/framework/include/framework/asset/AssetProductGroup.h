#pragma once

#include <core/file/FileSystem.h>
#include <core/util/Uuid.h>
#include <span>

namespace sky {

    enum class ProductError { None, NotFound, InvalidData, Unsupported, IOError, Busy, StaleRequest, OwnerConflict };

    struct ProductStatus {
        ProductError error = ProductError::None;
        std::string  message;
        explicit     operator bool() const
        {
            return error == ProductError::None;
        }
    };

    struct ProductGroupMember {
        Uuid     uuid;
        uint64_t size     = 0;
        uint64_t checksum = 0;
    };

    struct ProductGroupSnapshot {
        Uuid                            owner;
        Uuid                            generation;
        uint64_t                        revision = 0;
        std::vector<ProductGroupMember> members;
    };
    using ProductGroupSnapshotPtr = std::shared_ptr<const ProductGroupSnapshot>;

    class ProductGroupTransaction {
    public:
        ProductGroupTransaction() = default;
        const Uuid &GetOwner() const
        {
            return owner;
        }

    private:
        friend class AssetProductGroupStore;
        FileSystemPtr                   filesystem;
        Uuid                            owner;
        Uuid                            generation;
        std::vector<ProductGroupMember> members;
        std::vector<Uuid>               expectedMembers;
        bool                            committed = false;
        bool                            failed    = false;
    };

    // Only current.bin is a commit point. Member files and ownership records are immutable.
    class AssetProductGroupStore {
    public:
        explicit AssetProductGroupStore(FileSystemPtr filesystem) : fs(std::move(filesystem))
        {
        }

        ProductStatus Begin(const Uuid &owner, std::span<const Uuid> members, ProductGroupTransaction &out) const;
        ProductStatus Stage(ProductGroupTransaction &transaction, const Uuid &member, std::span<const uint8_t> bytes) const;
        ProductStatus Commit(ProductGroupTransaction &transaction, ProductGroupSnapshotPtr &out) const;
        ProductStatus Resolve(const Uuid &member, ProductGroupSnapshotPtr &out) const;
        ProductStatus Pin(const Uuid &owner, ProductGroupSnapshotPtr &out) const;
        ProductStatus Open(const ProductGroupSnapshot &snapshot, const Uuid &member, FilePtr &out) const;
        bool          HasOwnerRecord(const Uuid &member) const;
        bool          HasLegacyFallback(const Uuid &member) const;

    private:
        FileSystemPtr fs;
    };
} // namespace sky
