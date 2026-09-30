//
// ShaderFileSystem implementation: a MultiFileSystem that also serves
// in-memory virtual files.
//

#include <aurora/shader/ShaderFileSystem.h>

#include <utility>

namespace sky::aurora {

    ShaderFileSystem::~ShaderFileSystem() = default;

    void ShaderFileSystem::AddSearchPath(std::string directory)
    {
        AddFileSystem(new sky::NativeFileSystem(sky::FilePath(std::move(directory))));
    }

    void ShaderFileSystem::AddVirtualFile(std::string path, std::string content)
    {
        mFiles[std::move(path)] = std::move(content);
    }

    bool ShaderFileSystem::ReadFile(const std::string &path, std::string &content)
    {
        return Resolve(path, content);
    }

    bool ShaderFileSystem::Resolve(const std::string &path, std::string &content)
    {
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
