//
// ShaderFileSystem implementation: a MultiFileSystem that also serves
// in-memory virtual files.
//

#include <aurora/shader/ShaderFileSystem.h>
#include <aurora/shader/ShaderHash.h>

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace sky::aurora {

    ShaderFileSystem::~ShaderFileSystem() = default;

    void ShaderFileSystem::AddSearchPath(std::string directory)
    {
        AddFileSystem(new sky::NativeFileSystem(sky::FilePath(std::move(directory))));
    }

    bool ShaderFileSystem::AddVirtualFile(std::string path, std::string content)
    {
        if (!IsShaderLogicalPath(path) || IsBundlePath(path)) {
            return false;
        }
        mFiles[std::move(path)] = std::move(content);
        return true;
    }

    namespace {
        std::string PathKey(std::string path)
        {
            for (auto &character : path) {
                if (character >= 'A' && character <= 'Z') {
                    character = static_cast<char>(character + ('a' - 'A'));
                }
            }
            return path;
        }
        bool Within(const std::string &path, const std::string &owner)
        {
            return path.starts_with(owner + "/");
        }
        bool Fail(std::string *error, std::string message)
        {
            if (error != nullptr) {
                *error = std::move(message);
            }
            return false;
        }
        void Normalize(ShaderSourceBundle &bundle)
        {
            std::ranges::sort(bundle.dependencies);
            std::ranges::sort(bundle.includeRoots);
            std::ranges::sort(bundle.files, {}, &ShaderSourceFile::path);
            for (auto &file : bundle.files) {
                size_t newline = 0;
                while ((newline = file.content.find("\r\n", newline)) != std::string::npos) {
                    file.content.erase(newline, 1);
                    ++newline;
                }
            }
        }
        bool ValidateFiles(const ShaderSourceBundle &bundle, const std::unordered_map<std::string, std::string> &looseFiles, std::string *error)
        {
            if (!IsShaderLogicalPath(bundle.owner) || bundle.files.empty()) {
                return Fail(error, "Invalid or empty shader bundle: " + bundle.owner);
            }
            if (std::ranges::any_of(looseFiles, [&](const auto &file) { return Within(PathKey(file.first), PathKey(bundle.owner)); })) {
                return Fail(error, "Shader owner overlaps an existing loose source: " + bundle.owner);
            }
            std::unordered_set<std::string> paths;
            for (const auto &file : bundle.files) {
                if (!IsShaderLogicalPath(file.path) || !Within(file.path, bundle.owner) || !paths.insert(PathKey(file.path)).second) {
                    return Fail(error, "Invalid or duplicate shader path: " + file.path);
                }
            }
            paths.clear();
            for (const auto &root : bundle.includeRoots) {
                if (!IsShaderLogicalPath(root) || !Within(root, bundle.owner) || !paths.insert(PathKey(root)).second ||
                    std::ranges::none_of(bundle.files, [&](const auto &file) { return Within(file.path, root); })) {
                    return Fail(error, "Invalid or repeated shader include root: " + root);
                }
            }
            return true;
        }
    } // namespace

    bool ShaderFileSystem::IsBundlePath(const std::string &path) const
    {
        return std::ranges::any_of(bundles, [&](const auto &entry) { return Within(PathKey(path), PathKey(entry.first)); });
    }

    bool ShaderFileSystem::AddBundles(std::span<const ShaderSourceBundle> incoming, std::string *error)
    {
        auto staged = bundles;
        for (const auto &bundle : incoming) {
            if (!ValidateFiles(bundle, mFiles, error)) {
                return false;
            }
            for (const auto &[owner, existing] : staged) {
                const auto existingKey = PathKey(owner);
                const auto incomingKey = PathKey(bundle.owner);
                if (existingKey == incomingKey || Within(existingKey, incomingKey) || Within(incomingKey, existingKey)) {
                    return Fail(error, "Shader owner collision: " + bundle.owner);
                }
            }
            auto owned = std::make_shared<ShaderSourceBundle>(bundle);
            Normalize(*owned);
            staged.emplace(bundle.owner, std::move(owned));
        }
        for (const auto &[owner, bundle] : staged) {
            std::unordered_set<std::string> dependencies;
            for (const auto &dependency : bundle->dependencies) {
                if (!staged.contains(dependency) || !dependencies.insert(dependency).second) {
                    return Fail(error, std::string("Missing or repeated shader dependency ").append(dependency).append(" for ").append(owner));
                }
            }
        }
        std::unordered_set<std::string> resolved;
        while (resolved.size() < staged.size()) {
            const auto previous = resolved.size();
            for (const auto &[owner, bundle] : staged) {
                if (std::ranges::all_of(bundle->dependencies, [&](const auto &dependency) { return resolved.contains(dependency); })) {
                    resolved.insert(owner);
                }
            }
            if (resolved.size() == previous) {
                return Fail(error, "Missing or cyclic shader bundle dependency");
            }
        }
        for (const auto &[owner, bundle] : staged) {
            std::unordered_set<std::string> scope;
            std::vector<std::string>        pending{owner};
            while (!pending.empty()) {
                auto next = std::move(pending.back());
                pending.pop_back();
                if (scope.insert(next).second) {
                    const auto &dependencies = staged.at(next)->dependencies;
                    pending.insert(pending.end(), dependencies.begin(), dependencies.end());
                }
            }
            std::vector<std::string> roots{""};
            for (const auto &name : scope) {
                const auto &includes = staged.at(name)->includeRoots;
                roots.insert(roots.end(), includes.begin(), includes.end());
            }
            std::unordered_map<std::string, std::string> aliases;
            for (const auto &name : scope) {
                for (const auto &file : staged.at(name)->files) {
                    for (const auto &root : roots) {
                        if (!root.empty() && !Within(file.path, root)) {
                            continue;
                        }
                        auto alias                = PathKey(root.empty() ? file.path : file.path.substr(root.size() + 1));
                        const auto [found, added] = aliases.emplace(std::move(alias), file.path);
                        if (!added && found->second != file.path) {
                            return Fail(error, "Ambiguous shader include: " + found->first + " in " + owner);
                        }
                    }
                }
            }
        }
        bundles = std::move(staged);
        return true;
    }

    bool ShaderFileSystem::RemoveBundle(const std::string &owner, std::string *error)
    {
        for (const auto &[name, bundle] : bundles) {
            if (std::ranges::find(bundle->dependencies, owner) != bundle->dependencies.end()) {
                return Fail(error, "Shader bundle is still required by: " + name);
            }
        }
        return bundles.erase(owner) != 0 || Fail(error, "Shader owner not registered: " + owner);
    }

    bool ShaderFileSystem::LoadBundles(sky::IFileSystem &root, const std::string &owner, std::string *error)
    {
        std::vector<ShaderSourceBundle> loaded;
        if (bundles.contains(owner)) {
            return Fail(error, "Shader owner already registered: " + owner);
        }
        if (!LoadShaderSourceBundles(root, owner, loaded, error)) {
            return false;
        }
        for (auto &bundle : loaded) {
            Normalize(bundle);
            const auto found = bundles.find(bundle.owner);
            if (found != bundles.end() && *found->second != bundle) {
                return Fail(error, "Conflicting shared shader dependency: " + bundle.owner);
            }
        }
        std::erase_if(loaded, [&](const auto &bundle) { return bundles.contains(bundle.owner); });
        return AddBundles(loaded, error);
    }

    std::shared_ptr<ShaderFileSystem> ShaderFileSystem::Snapshot(const std::string &rootPath) const
    {
        auto                     result = std::make_shared<ShaderFileSystem>();
        std::vector<std::string> pending;
        for (const auto &[owner, bundle] : bundles) {
            if (Within(rootPath, owner)) {
                pending.push_back(owner);
            }
        }
        if (pending.empty()) {
            result->mFiles = mFiles;
        }
        while (!pending.empty()) {
            const auto owner = std::move(pending.back());
            pending.pop_back();
            const auto found = bundles.find(owner);
            if (found != bundles.end() && result->bundles.emplace(owner, found->second).second) {
                pending.insert(pending.end(), found->second->dependencies.begin(), found->second->dependencies.end());
            }
        }
        return result;
    }

    bool ShaderFileSystem::ReadFile(const std::string &path, std::string &content)
    {
        return Resolve(path, content);
    }

    uint64_t ShaderFileSystem::BundleFingerprint() const
    {
        std::vector<const ShaderSourceBundle *> ordered;
        ordered.reserve(bundles.size());
        for (const auto &[owner, bundle] : bundles) {
            ordered.push_back(bundle.get());
        }
        std::ranges::sort(ordered, {}, &ShaderSourceBundle::owner);
        uint64_t   hash = kFnv1aBasis;
        const auto mix  = [&](const std::string &value) { hash = HashMixBytes(HashMixU64(hash, value.size()), value); };
        for (const auto *bundle : ordered) {
            mix(bundle->owner);
            auto dependencies = bundle->dependencies;
            std::ranges::sort(dependencies);
            hash = HashMixU64(hash, dependencies.size());
            for (const auto &dependency : dependencies) {
                mix(dependency);
            }
            hash = HashMixU64(hash, bundle->includeRoots.size());
            for (const auto &root : bundle->includeRoots) {
                mix(root);
            }
            std::vector<const ShaderSourceFile *> files;
            files.reserve(bundle->files.size());
            for (const auto &file : bundle->files) {
                files.push_back(&file);
            }
            std::ranges::sort(files, {}, &ShaderSourceFile::path);
            hash = HashMixU64(hash, files.size());
            for (const auto *file : files) {
                mix(file->path);
                mix(file->content);
            }
        }
        return hash;
    }

    std::vector<std::string> ShaderFileSystem::IncludeRoots() const
    {
        std::vector<std::string> result{""};
        for (const auto &[owner, bundle] : bundles) {
            result.insert(result.end(), bundle->includeRoots.begin(), bundle->includeRoots.end());
        }
        std::ranges::sort(result);
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }

    bool ShaderFileSystem::Resolve(const std::string &path, std::string &content)
    {
        if (!IsShaderLogicalPath(path)) {
            return false;
        }
        for (const auto &[owner, bundle] : bundles) {
            if (Within(path, owner)) {
                const auto found = std::ranges::find(bundle->files, path, &ShaderSourceFile::path);
                if (found == bundle->files.end()) {
                    return false;
                }
                content = found->content;
                return true;
            }
        }
        if (IsBundlePath(path)) {
            return false;
        }
        const auto it = mFiles.find(path);
        if (it != mFiles.end()) {
            content = it->second;
            return true;
        }
        auto file = MultiFileSystem::OpenFile(sky::FilePath(path));
        if (file != nullptr) {
            return file->ReadString(content);
        }
        return false;
    }

} // namespace sky::aurora
