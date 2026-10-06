//
// Created on 2026/10/06.
//

#include <editor/core/filebrowser/FileSystemSource.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>

namespace sky::editor {

    namespace {
        std::string EnvironmentPath(const char *name)
        {
            const char *value = std::getenv(name);
            return value != nullptr ? std::string(value) : std::string();
        }
    } // namespace

    std::string FileSystemSource::Root() const
    {
        const std::string home = !EnvironmentPath("USERPROFILE").empty() ? EnvironmentPath("USERPROFILE") : EnvironmentPath("HOME");
        if (!home.empty()) {
            return home;
        }
        std::error_code ec;
        return std::filesystem::current_path(ec).string();
    }

    std::string FileSystemSource::Parent(const std::string &location) const
    {
        const std::filesystem::path path(location);
        if (path.empty()) {
            return location;
        }
        const std::filesystem::path parent = path.parent_path();
        return parent.empty() ? location : parent.string();
    }

    std::string FileSystemSource::Join(const std::string &location, const std::string &name) const
    {
        return (std::filesystem::path(location) / name).string();
    }

    bool FileSystemSource::IsDirectory(const std::string &location) const
    {
        std::error_code ec;
        return std::filesystem::is_directory(location, ec);
    }

    bool FileSystemSource::List(const std::string &location, std::vector<FileBrowserEntry> &out, std::string &error) const
    {
        out.clear();

        std::error_code                     ec;
        std::filesystem::directory_iterator iter(location, ec);
        if (ec) {
            error = "Cannot read directory";
            return false;
        }

        for (const auto &entry : iter) {
            std::error_code   entryError;
            const std::string name = entry.path().filename().string();
            if (name.empty()) {
                continue;
            }
            const bool       isDirectory = entry.is_directory(entryError);
            FileBrowserEntry item;
            item.name        = name;
            item.path        = entry.path().string();
            item.isDirectory = isDirectory;
            out.push_back(std::move(item));
        }

        std::sort(out.begin(), out.end(), [](const FileBrowserEntry &a, const FileBrowserEntry &b) {
            if (a.isDirectory != b.isDirectory) {
                return a.isDirectory;
            }
            return a.name < b.name;
        });
        error.clear();
        return true;
    }

    bool FileSystemSource::CreateDirectory(const std::string &location, const std::string &name, std::string &error)
    {
        if (name.empty()) {
            error = "Empty folder name";
            return false;
        }
        std::error_code             ec;
        const std::filesystem::path path = std::filesystem::path(location) / name;
        if (!std::filesystem::create_directory(path, ec)) {
            error = ec ? ec.message() : "Cannot create folder";
            return false;
        }
        error.clear();
        return true;
    }

    bool FileSystemSource::Rename(const std::string &location, const std::string &oldName, const std::string &newName, std::string &error)
    {
        if (oldName.empty() || newName.empty()) {
            error = "Invalid name";
            return false;
        }
        if (oldName == newName) {
            error.clear();
            return true;
        }
        std::error_code             ec;
        const std::filesystem::path from = std::filesystem::path(location) / oldName;
        const std::filesystem::path to   = std::filesystem::path(location) / newName;
        if (std::filesystem::exists(to, ec)) {
            error = "Already exists: " + newName;
            return false;
        }
        std::filesystem::rename(from, to, ec);
        if (ec) {
            error = ec.message();
            return false;
        }
        error.clear();
        return true;
    }

    std::vector<FileBrowserPlace> FileSystemSource::Places() const
    {
        std::vector<FileBrowserPlace> places;
        const std::string             home = !EnvironmentPath("USERPROFILE").empty() ? EnvironmentPath("USERPROFILE") : EnvironmentPath("HOME");
        if (!home.empty() && IsDirectory(home)) {
            places.push_back({"Home", home});
        }

#if defined(_WIN32)
        for (char drive = 'C'; drive <= 'Z'; ++drive) {
            const std::string root = std::string(1, drive) + ":\\";
            if (IsDirectory(root)) {
                places.push_back({std::string(1, drive) + ":", root});
            }
        }
#endif
        return places;
    }

} // namespace sky::editor
