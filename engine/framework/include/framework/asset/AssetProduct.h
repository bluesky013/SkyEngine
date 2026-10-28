#pragma once

#include <core/file/FileSystem.h>
#include <core/util/Uuid.h>
#include <cstdint>
#include <string>

namespace sky {
    inline constexpr uint32_t ASSET_PRODUCT_MAGIC        = 0x44525053;
    inline constexpr uint32_t ASSET_PRODUCT_VERSION      = 1;
    inline constexpr uint64_t ASSET_PRODUCT_HEADER_BYTES = 40;

    enum class ProductError { None, NotFound, InvalidData, Unsupported, IOError, Busy, OwnerConflict };

    struct ProductStatus {
        ProductError error = ProductError::None;
        std::string  message;
        explicit     operator bool() const
        {
            return error == ProductError::None;
        }
    };

    FilePath ProductFilePath(const Uuid &uuid);
} // namespace sky
