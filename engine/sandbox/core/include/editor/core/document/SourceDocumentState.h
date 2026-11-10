//
// Created on 2026/11/10.
//

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace sky::editor {
    class CommandService;
    class Document;

    // Checkpoints contain canonical source text; codecs and domain validation stay with the document.
    class SourceDocumentState {
    public:
        void     Loaded(Document &document, CommandService &commands, std::string canonical);
        void     Saved(Document &document, std::string canonical);
        void     Changed(Document &document, std::string_view canonical, bool valid);
        bool     BeginSave(std::string &error);
        void     RequestSave();
        bool     IsSavePending() const;
        bool     IsLoaded() const;
        uint64_t Revision() const;

    private:
        std::string saved;
        uint64_t    revision    = 0;
        bool        loaded      = false;
        bool        savePending = false;
    };
} // namespace sky::editor
