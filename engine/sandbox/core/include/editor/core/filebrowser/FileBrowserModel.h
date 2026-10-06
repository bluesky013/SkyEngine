//
// Created on 2026/10/06.
//

#pragma once

#include <editor/core/filebrowser/FileSystemSource.h>
#include <editor/core/filebrowser/IFileBrowserSource.h>

#include <string>
#include <vector>

namespace sky::editor {

    // Headless, UI-free file browser state: current location, the (filtered)
    // entries, navigation, the active filter, the editable name, and the resolved
    // result. Backed by an IFileBrowserSource (filesystem by default).
    class FileBrowserModel {
    public:
        FileBrowserModel()  = default;
        ~FileBrowserModel() = default;

        void                      SetSource(IFileBrowserSource *source);
        void                      SetRequest(const FileBrowserRequest &request);
        const FileBrowserRequest &GetRequest() const
        {
            return request;
        }
        FileBrowserMode GetMode() const
        {
            return request.mode;
        }

        bool SetDirectory(const std::string &location);
        bool NavigateTo(const std::string &name);
        bool NavigateToParent();

        // Creates a folder in the current location (a unique name when empty),
        // reloads, selects it, and puts its name in the name field. On failure
        // GetError() is set.
        bool CreateFolder(std::string name = {});

        // Renames the selected entry within the current location and reselects it.
        bool RenameSelected(const std::string &newName);

        const std::string &GetLocation() const
        {
            return location;
        }
        const std::vector<FileBrowserEntry> &GetEntries() const
        {
            return visible;
        }
        const std::vector<FileBrowserPlace> &GetPlaces() const
        {
            return places;
        }

        void SetSelected(int index);
        int  GetSelected() const
        {
            return selected;
        }
        const FileBrowserEntry *GetSelectedEntry() const;

        void SetName(std::string name)
        {
            nameField = std::move(name);
        }
        const std::string &GetName() const
        {
            return nameField;
        }

        // Filters: index -1 means "All". Selected index is one of request.filters.
        const std::vector<FileBrowserFilter> &GetFilters() const
        {
            return request.filters;
        }
        int GetActiveFilter() const
        {
            return activeFilter;
        }
        void        SetActiveFilter(int index);
        std::string GetFilterLabel(int index) const;

        const std::string &GetError() const
        {
            return error;
        }

        bool        AcceptsFileName(const std::string &fileName) const;
        bool        CanAccept() const;
        std::string ResultPath() const;

    private:
        bool PassesFilter(const FileBrowserEntry &entry) const;
        void ApplyFilter();
        void Load();

        FileSystemSource              defaultSource;
        IFileBrowserSource           *source = nullptr;
        FileBrowserRequest            request;
        std::string                   location;
        std::vector<FileBrowserEntry> entries; // all children
        std::vector<FileBrowserEntry> visible; // after filtering
        std::vector<FileBrowserPlace> places;
        std::string                   nameField;
        std::string                   error;
        int                           selected     = -1;
        int                           activeFilter = -1;
    };

} // namespace sky::editor
