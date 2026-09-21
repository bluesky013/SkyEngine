//
// Created on 2026/09/21.
//

#pragma once

#include <editor/core/document/Document.h>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace sky::editor {

    // Toolkit-independent document service.
    //
    // Maps an asset identifier to its editable document, creating it through a
    // registered factory on first open. The service owns the open documents.
    class DocumentService {
    public:
        using Factory = std::function<DocumentPtr(const std::string &assetId)>;

        DocumentService() = default;
        ~DocumentService() = default;

        DocumentService(const DocumentService &) = delete;
        DocumentService &operator=(const DocumentService &) = delete;

        // Registers a document factory for a document type (for example, a file
        // extension or asset type).
        void RegisterFactory(std::string type, Factory factory);

        // Opens the document for an asset, creating it if not already open.
        // Returns nullptr when no factory matches the requested type.
        Document *Open(const std::string &assetId, const std::string &type);
        Document *Find(const std::string &assetId) const;

        bool Close(const std::string &assetId);
        void CloseAll();

        const std::vector<DocumentPtr> &GetDocuments() const { return documents; }

    private:
        std::unordered_map<std::string, Factory> factories;
        std::unordered_map<std::string, DocumentPtr> byAsset;
        std::vector<DocumentPtr> documents;
    };

} // namespace sky::editor
