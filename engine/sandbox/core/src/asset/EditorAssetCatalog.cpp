//
// Created on 2026/10/04.
//

#include <editor/core/asset/EditorAssetCatalog.h>
#include <framework/asset/AssetDataBase.h>

namespace sky::editor {

    namespace {

        class AssetDatabaseCatalog : public IEditorAssetCatalog {
        public:
            std::vector<EditorAssetItem> Gather(const std::string &type) const override
            {
                std::vector<EditorAssetItem> out;
                sky::AssetDataBase *db = sky::AssetDataBase::Get();
                if (db == nullptr) {
                    return out;
                }
                for (const auto &source : db->Gather(type)) {
                    if (!source) {
                        continue;
                    }
                    EditorAssetItem item;
                    item.uuid = source->uuid;
                    item.name = source->name.empty() ? source->path.FileName() : source->name;
                    item.path = source->path.GetStr();
                    item.type = type;
                    out.push_back(std::move(item));
                }
                return out;
            }

            bool GetType(const Uuid &uuid, std::string &outType) const override
            {
                sky::AssetDataBase *db = sky::AssetDataBase::Get();
                return db != nullptr && db->GetType(uuid, outType);
            }

            bool GetName(const Uuid &uuid, std::string &outName) const override
            {
                sky::AssetDataBase *db = sky::AssetDataBase::Get();
                if (db == nullptr) {
                    return false;
                }
                const auto source = db->FindAsset(uuid);
                if (!source) {
                    return false;
                }
                outName = source->name.empty() ? source->path.FileName() : source->name;
                return true;
            }
        };

        IEditorAssetCatalog *g_catalog = nullptr;

    } // namespace

    IEditorAssetCatalog *GetEditorAssetCatalog()
    {
        static AssetDatabaseCatalog defaultCatalog;
        return g_catalog != nullptr ? g_catalog : &defaultCatalog;
    }

    void SetEditorAssetCatalog(IEditorAssetCatalog *catalog)
    {
        g_catalog = catalog;
    }

} // namespace sky::editor
