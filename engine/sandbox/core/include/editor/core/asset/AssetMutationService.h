//
// Editor-side write operations on the source namespace (Import / Move / Duplicate / Delete), split out of
// the read model (EditorAssetCatalog). Every operation is gated to writable mounts and refreshes the
// catalog afterwards. Process-wide (cross-DLL) service.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/util/Uuid.h>

#include <string>

namespace sky::editor {

    class AssetMutationService : public sky::Singleton<AssetMutationService> {
        friend class sky::Singleton<AssetMutationService>;

    public:
        Uuid Import(const std::string &sourceFile, const std::string &destPath, bool cook);
        bool Move(const std::string &from, const std::string &to);
        Uuid Duplicate(const std::string &from, const std::string &to);
        bool Delete(const Uuid &uuid);

    private:
        // Resolves a display path ("Project/textures/a.png") to a mount-relative path when its mount is
        // writable; false otherwise. Gates every mutating operation.
        static bool WritableRelative(const std::string &display, std::string &out);
    };

} // namespace sky::editor
