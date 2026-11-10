//
// Created on 2026/11/10.
//

#pragma once

#include <core/file/FileTransaction.h>
#include <cstddef>
#include <functional>
#include <string_view>

namespace sky {
    class Uuid;
}

namespace sky::editor {
    struct SourceDocumentStorage {
        std::function<FileOperationStatus(std::string &)>    read;
        std::function<FileOperationStatus(std::string_view)> write;
        std::function<void(const Uuid &)>                    requestCook;
    };

    SourceDocumentStorage MakeAssetSourceStorage(const Uuid &asset, size_t maxBytes);
} // namespace sky::editor
