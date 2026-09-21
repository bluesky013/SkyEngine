//
// Created on 2026/09/21.
//

#pragma once

#include <memory>
#include <string>
#include <utility>

namespace sky::editor {

    // Base class for an editable document. Dirty tracking is owned here; the
    // concrete document provides Load/Save. No UI-toolkit dependency.
    class Document {
    public:
        explicit Document(std::string path) : path(std::move(path)) {}
        virtual ~Document() = default;

        Document(const Document &) = delete;
        Document &operator=(const Document &) = delete;

        const std::string &GetPath() const { return path; }
        bool IsDirty() const { return dirty; }
        void MarkDirty() { dirty = true; }
        void ClearDirty() { dirty = false; }

        virtual bool Load() { return true; }
        virtual bool Save() { return true; }

    private:
        std::string path;
        bool dirty = false;
    };

    using DocumentPtr = std::shared_ptr<Document>;

} // namespace sky::editor
