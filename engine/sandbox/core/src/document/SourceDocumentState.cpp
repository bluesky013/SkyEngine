//
// Created on 2026/11/10.
//

#include <editor/core/document/SourceDocumentState.h>

#include <editor/core/command/CommandService.h>
#include <editor/core/document/Document.h>

namespace sky::editor {
    void SourceDocumentState::Loaded(Document &document, CommandService &commands, std::string canonical)
    {
        loaded = true;
        Saved(document, std::move(canonical));
        commands.Clear();
    }
    void SourceDocumentState::Saved(Document &document, std::string canonical)
    {
        saved       = std::move(canonical);
        savePending = false;
        document.ClearDirty();
        ++revision;
    }
    void SourceDocumentState::Changed(Document &document, std::string_view canonical, bool valid)
    {
        if (loaded && valid && canonical == saved)
            document.ClearDirty();
        else
            document.MarkDirty();
        ++revision;
    }
    bool SourceDocumentState::BeginSave(std::string &error)
    {
        savePending = false;
        if (!loaded && error.empty())
            error = "Source must load successfully before saving";
        return loaded;
    }
    void SourceDocumentState::RequestSave()
    {
        savePending = true;
    }
    bool SourceDocumentState::IsSavePending() const
    {
        return savePending;
    }
    bool SourceDocumentState::IsLoaded() const
    {
        return loaded;
    }
    uint64_t SourceDocumentState::Revision() const
    {
        return revision;
    }
} // namespace sky::editor
