//
// Created on 2026/10/06.
//

#include <editor/core/filebrowser/FileBrowserModel.h>

#include <algorithm>
#include <cctype>

namespace sky::editor {

    namespace {
        std::string ToLower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        std::string ExtensionOf(const std::string &fileName)
        {
            const std::size_t dot = fileName.find_last_of('.');
            if (dot == std::string::npos || dot + 1 >= fileName.size()) {
                return std::string();
            }
            return ToLower(fileName.substr(dot + 1));
        }

        bool FilterMatchesName(const FileBrowserFilter &filter, const std::string &fileName)
        {
            if (filter.extensions.empty() && filter.assetTypes.empty()) {
                return true;
            }
            const std::string ext = ExtensionOf(fileName);
            for (const auto &candidate : filter.extensions) {
                if (ToLower(candidate) == ext && !ext.empty()) {
                    return true;
                }
            }
            return false;
        }
    } // namespace

    void FileBrowserModel::SetSource(IFileBrowserSource *inSource)
    {
        source = inSource != nullptr ? inSource : &defaultSource;
    }

    void FileBrowserModel::SetRequest(const FileBrowserRequest &inRequest)
    {
        if (source == nullptr) {
            source = &defaultSource;
        }
        request   = inRequest;
        nameField = request.defaultName;
        selected  = -1;
        error.clear();
        entries.clear();
        visible.clear();

        places       = !request.places.empty() ? request.places : source->Places();
        activeFilter = request.filters.empty() ? -1 : 0;

        const std::string start = request.directory.empty() ? source->Root() : request.directory;
        SetDirectory(start);
    }

    bool FileBrowserModel::SetDirectory(const std::string &inLocation)
    {
        if (source == nullptr) {
            source = &defaultSource;
        }
        if (!source->IsDirectory(inLocation)) {
            error = "Not a directory: " + inLocation;
            return false;
        }
        location = inLocation;
        selected = -1;
        error.clear();
        Load();
        return true;
    }

    bool FileBrowserModel::NavigateTo(const std::string &name)
    {
        for (const auto &entry : entries) {
            if (entry.name == name && entry.isDirectory) {
                return SetDirectory(entry.path);
            }
        }
        for (std::size_t i = 0; i < visible.size(); ++i) {
            if (visible[i].name == name && !visible[i].isDirectory) {
                SetSelected(static_cast<int>(i));
                SetName(name);
                return true;
            }
        }
        return false;
    }

    bool FileBrowserModel::NavigateToParent()
    {
        if (source == nullptr || location.empty()) {
            return false;
        }
        const std::string parent = source->Parent(location);
        if (parent == location || parent.empty()) {
            return false;
        }
        return SetDirectory(parent);
    }

    bool FileBrowserModel::CreateFolder(std::string name)
    {
        if (source == nullptr) {
            source = &defaultSource;
        }
        if (name.empty()) {
            name      = "New Folder";
            int index = 2;
            while (source->IsDirectory(source->Join(location, name))) {
                name = "New Folder " + std::to_string(index++);
            }
        }

        std::string message;
        if (!source->CreateDirectory(location, name, message)) {
            error = message.empty() ? "Cannot create folder" : message;
            return false;
        }

        const std::string target = source->Join(location, name);
        SetDirectory(location); // reload; clears error
        for (std::size_t i = 0; i < visible.size(); ++i) {
            if (visible[i].isDirectory && visible[i].path == target) {
                SetSelected(static_cast<int>(i));
                break;
            }
        }
        nameField = name;
        return true;
    }

    bool FileBrowserModel::RenameSelected(const std::string &newName)
    {
        if (source == nullptr) {
            source = &defaultSource;
        }
        const FileBrowserEntry *entry = GetSelectedEntry();
        if (entry == nullptr || newName.empty()) {
            return false;
        }
        const std::string oldName = entry->name;
        if (oldName == newName) {
            nameField = newName;
            return true;
        }

        std::string message;
        if (!source->Rename(location, oldName, newName, message)) {
            error = message.empty() ? "Cannot rename" : message;
            return false;
        }

        const std::string target = source->Join(location, newName);
        SetDirectory(location); // reload; clears error
        for (std::size_t i = 0; i < visible.size(); ++i) {
            if (visible[i].path == target) {
                SetSelected(static_cast<int>(i));
                break;
            }
        }
        nameField = newName;
        return true;
    }

    void FileBrowserModel::SetSelected(int index)
    {
        if (index < -1 || index >= static_cast<int>(visible.size())) {
            return;
        }
        selected = index;
    }

    const FileBrowserEntry *FileBrowserModel::GetSelectedEntry() const
    {
        if (selected < 0 || selected >= static_cast<int>(visible.size())) {
            return nullptr;
        }
        return &visible[static_cast<std::size_t>(selected)];
    }

    void FileBrowserModel::SetActiveFilter(int index)
    {
        if (index < -1 || index >= static_cast<int>(request.filters.size())) {
            return;
        }
        activeFilter = index;
        selected     = -1;
        ApplyFilter();
    }

    std::string FileBrowserModel::GetFilterLabel(int index) const
    {
        if (index < 0) {
            return "All Files (*.*)";
        }
        if (index < static_cast<int>(request.filters.size())) {
            return request.filters[static_cast<std::size_t>(index)].label;
        }
        return "Filter";
    }

    bool FileBrowserModel::AcceptsFileName(const std::string &fileName) const
    {
        if (fileName.empty()) {
            return false;
        }
        if (request.mode == FileBrowserMode::SELECT_DIRECTORY) {
            return true;
        }
        if (activeFilter < 0 || activeFilter >= static_cast<int>(request.filters.size())) {
            return true;
        }
        return FilterMatchesName(request.filters[static_cast<std::size_t>(activeFilter)], fileName);
    }

    bool FileBrowserModel::CanAccept() const
    {
        if (request.mode == FileBrowserMode::SELECT_DIRECTORY) {
            return !nameField.empty();
        }
        const FileBrowserEntry *entry = GetSelectedEntry();
        if (entry != nullptr && !entry->isDirectory) {
            return true;
        }
        return AcceptsFileName(nameField);
    }

    std::string FileBrowserModel::ResultPath() const
    {
        if (source == nullptr) {
            return std::string();
        }
        if (request.mode != FileBrowserMode::SELECT_DIRECTORY) {
            const FileBrowserEntry *entry = GetSelectedEntry();
            if (entry != nullptr && !entry->isDirectory) {
                return entry->path;
            }
        }
        return source->Join(location, nameField);
    }

    bool FileBrowserModel::PassesFilter(const FileBrowserEntry &entry) const
    {
        if (request.mode == FileBrowserMode::SELECT_DIRECTORY) {
            return entry.isDirectory;
        }
        if (entry.isDirectory) {
            return true;
        }
        if (activeFilter < 0 || activeFilter >= static_cast<int>(request.filters.size())) {
            return true;
        }
        const FileBrowserFilter &filter = request.filters[static_cast<std::size_t>(activeFilter)];
        if (filter.extensions.empty() && filter.assetTypes.empty()) {
            return true;
        }
        const std::string ext = ExtensionOf(entry.name);
        for (const auto &candidate : filter.extensions) {
            if (ToLower(candidate) == ext && !ext.empty()) {
                return true;
            }
        }
        for (const auto &candidate : filter.assetTypes) {
            if (!entry.typeId.empty() && candidate == entry.typeId) {
                return true;
            }
        }
        return false;
    }

    void FileBrowserModel::ApplyFilter()
    {
        visible.clear();
        for (const auto &entry : entries) {
            if (PassesFilter(entry)) {
                visible.push_back(entry);
            }
        }
        if (selected >= static_cast<int>(visible.size())) {
            selected = -1;
        }
    }

    void FileBrowserModel::Load()
    {
        entries.clear();
        std::string listError;
        if (source->List(location, entries, listError)) {
            error.clear();
        } else {
            error = listError;
        }
        ApplyFilter();
    }

} // namespace sky::editor
