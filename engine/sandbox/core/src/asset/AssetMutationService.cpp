//
// Asset mutation operations split out of EditorAssetCatalog (see AssetMutationService.h).
//

#include <editor/core/asset/AssetMutationService.h>

#include <editor/core/asset/EditorAssetCatalog.h>

#include <framework/asset/AssetDataBase.h>

#include <core/file/FileSystem.h>

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

} // namespace sky::editor
