//
// Asset mutation operations split out of EditorAssetCatalog (see AssetMutationService.h).
//

#include <editor/core/asset/AssetMutationService.h>

#include <editor/core/asset/EditorAssetCatalog.h>

#include <framework/asset/AssetDataBase.h>

#include <core/file/FileSystem.h>

#include <filesystem>
#include <vector>

namespace sky::editor {

    namespace {

        void SplitDisplay(const std::string &display, std::string &mount, std::string &relative)
        {
            const auto slash = display.find('/');
            if (slash == std::string::npos) {
                mount    = display;
                relative = {};
                return;
            }
            mount    = display.substr(0, slash);
            relative = display.substr(slash + 1);
        }

    } // namespace

    bool AssetMutationService::WritableRelative(const std::string &display, std::string &out)
    {
        auto *db = sky::AssetDataBase::Get();
        if (db == nullptr) {
            return false;
        }
        std::string mountName;
        std::string relative;
        SplitDisplay(display, mountName, relative);
        for (const auto &mount : db->GetMounts()) {
            if (mount.writable && mount.displayName == mountName && !relative.empty()) {
                out = relative;
                return true;
            }
        }
        return false;
    }

    Uuid AssetMutationService::Import(const std::string &sourceFile, const std::string &destPath, bool cook)
    {
        std::string relative;
        if (!WritableRelative(destPath, relative)) {
            return {};
        }
        auto *db = sky::AssetDataBase::Get();
        if (db == nullptr) {
            return {};
        }
        auto source = db->ImportAsset(sky::FilePath(sourceFile), sky::FilePath(relative), cook);
        EditorAssetCatalog::Get()->Refresh();
        return source != nullptr ? source->uuid : Uuid{};
    }

    bool AssetMutationService::Move(const std::string &from, const std::string &to)
    {
        std::string fromRel;
        std::string toRel;
        if (!WritableRelative(from, fromRel) || !WritableRelative(to, toRel)) {
            return false;
        }
        auto *db = sky::AssetDataBase::Get();
        if (db == nullptr) {
            return false;
        }
        const bool ok = db->MoveAsset(sky::FilePath(fromRel), sky::FilePath(toRel)) != nullptr;
        if (ok) {
            EditorAssetCatalog::Get()->Refresh();
        }
        return ok;
    }

    Uuid AssetMutationService::Duplicate(const std::string &from, const std::string &to)
    {
        std::string fromRel;
        std::string toRel;
        if (!WritableRelative(from, fromRel) || !WritableRelative(to, toRel)) {
            return {};
        }
        auto *db = sky::AssetDataBase::Get();
        if (db == nullptr) {
            return {};
        }
        auto source = db->DuplicateAsset(sky::FilePath(fromRel), sky::FilePath(toRel));
        EditorAssetCatalog::Get()->Refresh();
        return source != nullptr ? source->uuid : Uuid{};
    }

    bool AssetMutationService::Delete(const Uuid &uuid)
    {
        auto *db = sky::AssetDataBase::Get();
        if (db == nullptr) {
            return false;
        }
        auto source = db->FindAsset(uuid);
        if (!source) {
            return false;
        }
        bool writable = false;
        for (const auto &mount : db->GetMounts()) {
            if (mount.writable && mount.id == source->mount) {
                writable = true;
                break;
            }
        }
        if (!writable) {
            return false;
        }
        db->RemoveAsset(uuid);
        EditorAssetCatalog::Get()->Refresh();
        return true;
    }

    FileOperationStatus
    AssetMutationService::SaveGeneratedSource(const std::string &displayPath, const Uuid &expectedOwner, std::string_view contents, Uuid &out)
    {
        std::string relative;
        if (!WritableRelative(displayPath, relative) || contents.empty()) {
            return {FileOperationError::InvalidPath, "Generated source requires a writable mount and nonempty contents"};
        }
        const std::filesystem::path logical(relative);
        if (logical.is_absolute() || logical.has_root_name() || logical.lexically_normal().generic_string() != relative ||
            relative.find('\\') != std::string::npos) {
            return {FileOperationError::InvalidPath, "Generated source path must be canonical and mount-relative"};
        }
        for (const auto &part : logical) {
            if (part == "..") {
                return {FileOperationError::InvalidPath, "Generated source path escapes its mount"};
            }
        }
        auto      *database   = AssetDataBase::Get();
        const auto filesystem = database->GetWorkSpaceFs();
        if (!filesystem || filesystem->IsReadOnly()) {
            return {FileOperationError::InvalidPath, "Writable source filesystem is unavailable"};
        }
        const FilePath destination(relative);
        FileLockPtr    writeLock;
        if (const auto status = filesystem->AcquireFileLock(FilePath(relative + ".authoring.lock"), writeLock); !status) {
            return status;
        }
        const auto existing = database->FindAsset(destination);
        if ((expectedOwner && (!existing || existing->uuid != expectedOwner)) ||
            (!expectedOwner && (existing || filesystem->FileExist(destination)))) {
            return {FileOperationError::AlreadyExists, "Generated source destination does not match the expected saved owner"};
        }
        if (existing) {
            EditorAssetItem item;
            if (!EditorAssetCatalog::Get()->Find(existing->uuid, item) || !item.writable || item.path != displayPath) {
                return {FileOperationError::InvalidPath, "Saved source no longer belongs to the selected writable mount"};
            }
            if (const auto status = database->PersistSourceIdentity(existing->uuid); !status) {
                return status;
            }
        }
        const FilePath             temporary(relative + ".tmp-" + Uuid::Create().ToString());
        const std::vector<uint8_t> bytes(contents.begin(), contents.end());
        if (const auto status = filesystem->WriteFileExclusive(temporary, bytes); !status) {
            return status;
        }
        if (const auto status = filesystem->ReplaceFileAtomic(temporary, destination); !status) {
            const auto cleanup = filesystem->RemoveFile(temporary);
            return cleanup ? status : FileOperationStatus{cleanup.error, status.message + "; temporary cleanup failed: " + cleanup.message};
        }
        const auto source   = existing ? existing : database->RegisterAsset(destination, false);
        const auto identity = existing ? FileOperationStatus{}
                              : source ? database->PersistSourceIdentity(source->uuid)
                                       : FileOperationStatus{FileOperationError::IOError, "Generated source registration failed"};
        if (!identity) {
            if (!existing) {
                if (source) {
                    database->RemoveAsset(source->uuid);
                }
                const auto cleanup = filesystem->RemoveFile(destination);
                EditorAssetCatalog::Get()->Refresh();
                if (!cleanup) {
                    return {cleanup.error, identity.message + "; new source cleanup failed: " + cleanup.message};
                }
            }
            return identity;
        }
        out = source->uuid;
        EditorAssetCatalog::Get()->Refresh();
        return {};
    }

} // namespace sky::editor
