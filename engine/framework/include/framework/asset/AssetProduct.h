#pragma once

#include <core/file/FileSystem.h>
#include <core/util/Uuid.h>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sky {
    inline constexpr uint32_t ASSET_PRODUCT_MAGIC        = 0x44525053;
    inline constexpr uint32_t ASSET_PRODUCT_VERSION      = 1;
    inline constexpr uint64_t ASSET_PRODUCT_HEADER_BYTES = 40;

    enum class ProductError { None, NotFound, InvalidData, Unsupported, IOError, Busy, OwnerConflict, Stale };

    enum class AssetCommitState { NotCommitted, Committed, CommittedNeedsRepair, RollbackFailed };

    struct AssetRecoveryIssue {
        std::string         stage;
        FilePath            path;
        FileOperationStatus status;
    };

    std::string
    DescribeAssetOperation(std::string_view message, AssetCommitState state, std::string_view stage, std::span<const AssetRecoveryIssue> recovery);

    struct ProductStatus {
        ProductError                    error = ProductError::None;
        std::string                     message;
        AssetCommitState                state = AssetCommitState::NotCommitted;
        std::string                     stage;
        std::vector<AssetRecoveryIssue> recovery;
        std::string                     Describe() const
        {
            return DescribeAssetOperation(message, state, stage, recovery);
        }
        explicit operator bool() const
        {
            return error == ProductError::None;
        }
    };

    class AssetPublicationScope {
    public:
        virtual ~AssetPublicationScope() = default;
    };

    class AssetPublicationGuard {
    public:
        virtual ~AssetPublicationGuard()                                                 = default;
        virtual ProductStatus Acquire(std::unique_ptr<AssetPublicationScope> &out) const = 0;
        // The source transaction already owns this directory lock; validation must not acquire it again.
        virtual ProductStatus CheckSource(const Uuid &, const FileSystemPtr &, const FilePath &) const
        {
            return {ProductError::Unsupported, "Publication guard does not validate source mutations"};
        }
    };

    FilePath ProductFilePath(const Uuid &uuid);
} // namespace sky
