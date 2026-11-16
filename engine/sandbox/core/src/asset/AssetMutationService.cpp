//
// Asset mutation operations split out of EditorAssetCatalog (see AssetMutationService.h).
//

#include <editor/core/asset/AssetMutationService.h>

#include <editor/core/asset/EditorAssetCatalog.h>

#include <framework/asset/AssetDataBase.h>

#include <core/file/FileSystem.h>
#include <core/logger/Logger.h>

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
        if (!source)
            LOG_E("AssetMutation", "%s", source.Describe().c_str());
        return source.source ? source.source->uuid : Uuid{};
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
        const auto result = db->MoveAsset(sky::FilePath(fromRel), sky::FilePath(toRel));
        EditorAssetCatalog::Get()->Refresh();
        if (!result)
            LOG_E("AssetMutation", "%s", result.Describe().c_str());
        return static_cast<bool>(result);
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
        if (!source)
            LOG_E("AssetMutation", "%s", source.Describe().c_str());
        return source.source ? source.source->uuid : Uuid{};
    }

    AssetMutationResult AssetMutationService::Delete(const Uuid &uuid)
    {
        auto *db = sky::AssetDataBase::Get();
        if (!db)
            return {FileOperationError::InvalidPath, "Asset database is unavailable"};
        const auto status = db->DeleteAsset(uuid);
        EditorAssetCatalog::Get()->Refresh();
        return status;
    }

    FileOperationStatus
    AssetMutationService::SaveGeneratedSource(const std::string &displayPath, const Uuid &expectedOwner, std::string_view contents, Uuid &out)
    {
        std::string relative;
        if (!WritableRelative(displayPath, relative) || contents.empty()) {
            return {FileOperationError::InvalidPath, "Generated source requires a writable mount and nonempty contents"};
        }
        auto       *database = AssetDataBase::Get();
        std::string displayMount;
        std::string ignored;
        SplitDisplay(displayPath, displayMount, ignored);
        AssetDestination resolved;
        for (const auto &mount : database->GetMounts()) {
            if (mount.displayName == displayMount) {
                if (const auto status = database->ResolveWritableDestination(FilePath(relative), resolved, mount.id); !status)
                    return status;
                break;
            }
        }
        const auto filesystem = resolved.filesystem;
        if (!filesystem)
            return {FileOperationError::InvalidPath, "Writable source filesystem is unavailable"};
        const FilePath destination(relative);
        FileLockPtr    writeLock;
        if (const auto status = filesystem->AcquireFileLock(FilePath(relative + ".authoring.lock"), writeLock); !status) {
            return status;
        }
        const auto existing = database->FindAsset(resolved.mount, destination);
        if ((expectedOwner && (!existing || existing->uuid != expectedOwner)) ||
            (!expectedOwner && (existing || filesystem->FileExist(destination)))) {
            return {FileOperationError::AlreadyExists, "Generated source destination does not match the expected saved owner"};
        }
        if (existing) {
            EditorAssetItem item;
            if (!EditorAssetCatalog::Get()->Find(existing->uuid, item) || !item.writable || item.path != displayPath) {
                return {FileOperationError::InvalidPath, "Saved source no longer belongs to the selected writable mount"};
            }
            if (const auto status = database->PersistSourceIdentity(existing->uuid); !status)
                return {status.status.error, status.Describe()};
        }
        const std::vector<uint8_t> bytes(contents.begin(), contents.end());
        if (!existing) {
            const auto created = database->CreateAsset(resolved.mount, destination, bytes);
            EditorAssetCatalog::Get()->Refresh();
            if (!created)
                return {created.status.error, created.Describe()};
            out = created.source->uuid;
            return {};
        }
        if (const auto status = database->WriteSource(existing->uuid, contents); !status)
            return {status.status.error, status.Describe()};
        out = existing->uuid;
        EditorAssetCatalog::Get()->Refresh();
        return {};
    }

} // namespace sky::editor
