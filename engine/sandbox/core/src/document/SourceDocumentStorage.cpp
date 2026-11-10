//
// Created on 2026/11/10.
//

#include <editor/core/document/SourceDocumentStorage.h>

#include <core/file/FileSystem.h>
#include <core/file/FileUtil.h>
#include <editor/core/asset/AssetCookService.h>
#include <framework/asset/AssetDataBase.h>

#include <filesystem>
#include <fstream>

namespace sky::editor {
    SourceDocumentStorage MakeAssetSourceStorage(const Uuid &asset, size_t maxBytes)
    {
        return {[asset, maxBytes](std::string &out) -> FileOperationStatus {
                    std::string path;
                    if (!AssetDataBase::Get()->GetAbsoluteSourcePath(asset, path))
                        return {FileOperationError::InvalidPath, "Asset source is not registered"};
                    std::ifstream stream(PathFromUtf8(path), std::ios::binary | std::ios::ate);
                    if (!stream)
                        return {FileOperationError::IOError, "Cannot open asset source"};
                    const auto size = stream.tellg();
                    if (size <= 0 || static_cast<uint64_t>(size) > maxBytes)
                        return {FileOperationError::IOError, "Asset source size is invalid"};
                    std::string text(static_cast<size_t>(size), '\0');
                    stream.seekg(0);
                    if (!stream.read(text.data(), static_cast<std::streamsize>(size)))
                        return {FileOperationError::IOError, "Cannot read complete asset source"};
                    out = std::move(text);
                    return {};
                },
                [asset, maxBytes](std::string_view text) -> FileOperationStatus {
                    if (text.empty() || text.size() > maxBytes)
                        return {FileOperationError::IOError, "Asset source size is invalid"};
                    std::string path;
                    if (!AssetDataBase::Get()->GetAbsoluteSourcePath(asset, path))
                        return {FileOperationError::InvalidPath, "Asset source is not registered"};
                    const FilePath   target(path);
                    NativeFileSystem fs(target.Parent());
                    const FilePath   staging(target.FileName() + "." + Uuid::Create().ToString() + ".tmp");
                    auto             status = fs.WriteFileExclusive(staging, {reinterpret_cast<const uint8_t *>(text.data()), text.size()});
                    if (!status && status.error == FileOperationError::AlreadyExists)
                        return status;
                    if (status)
                        status = fs.ReplaceFileAtomic(staging, FilePath(target.FileName()));
                    if (!status) {
                        std::error_code error;
                        std::filesystem::remove(PathFromUtf8((target.Parent() / staging).GetStr()), error);
                        if (error)
                            status.message += "; staging cleanup failed: " + error.message();
                    }
                    return status;
                },
                [](const Uuid &id) { AssetCookService::Get()->RequestCook(id); }};
    }
} // namespace sky::editor
