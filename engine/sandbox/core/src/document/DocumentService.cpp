//
// Created on 2026/09/21.
//

#include <editor/core/document/DocumentService.h>
#include <algorithm>
#include <utility>

namespace sky::editor {

    void DocumentService::RegisterFactory(std::string type, Factory factory)
    {
        factories[std::move(type)] = std::move(factory);
    }

    Document *DocumentService::Open(const std::string &assetId, const std::string &type)
    {
        auto existing = byAsset.find(assetId);
        if (existing != byAsset.end()) {
            return existing->second.get();
        }

        auto factory = factories.find(type);
        if (factory == factories.end() || !factory->second) {
            return nullptr;
        }

        DocumentPtr document = factory->second(assetId);
        if (!document) {
            return nullptr;
        }

        Document *raw = document.get();
        byAsset.emplace(assetId, document);
        documents.push_back(std::move(document));
        return raw;
    }

    Document *DocumentService::Find(const std::string &assetId) const
    {
        auto iter = byAsset.find(assetId);
        return iter == byAsset.end() ? nullptr : iter->second.get();
    }

    bool DocumentService::Close(const std::string &assetId)
    {
        auto iter = byAsset.find(assetId);
        if (iter == byAsset.end()) {
            return false;
        }

        Document *target = iter->second.get();
        byAsset.erase(iter);
        documents.erase(std::remove_if(documents.begin(), documents.end(),
                                        [target](const DocumentPtr &document) { return document.get() == target; }),
                         documents.end());
        return true;
    }

    void DocumentService::CloseAll()
    {
        byAsset.clear();
        documents.clear();
    }

} // namespace sky::editor
