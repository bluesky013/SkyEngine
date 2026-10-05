//
// Created on 2026/10/04.
//

#pragma once

#include <core/util/Uuid.h>
#include <string>
#include <vector>

namespace sky::editor {

    struct EditorAssetItem {
        Uuid        uuid;
        std::string name;
        std::string path;
        std::string type;
    };

    // Toolkit-independent asset lookup used by asset-typed properties. The
    // default implementation is backed by the framework AssetDataBase; hosts may
    // install their own (e.g. for tests) through SetEditorAssetCatalog.
    class IEditorAssetCatalog {
    public:
        virtual ~IEditorAssetCatalog() = default;

        virtual std::vector<EditorAssetItem> Gather(const std::string &type) const = 0;
        virtual bool GetType(const Uuid &uuid, std::string &outType) const = 0;
        virtual bool GetName(const Uuid &uuid, std::string &outName) const = 0;
    };

    IEditorAssetCatalog *GetEditorAssetCatalog();
    void SetEditorAssetCatalog(IEditorAssetCatalog *catalog);

} // namespace sky::editor
