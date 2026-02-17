//
// Created by blues on 2024/12/30.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/file/FileSystem.h>
#include <core/name/Name.h>
#include <core/archive/MemoryArchive.h>
#include <taskflow/taskflow.hpp>
#include <shader/ShaderCompiler.h>

namespace sky {
    struct ShaderCacheEntry;

    class ShaderFileSystem : public Singleton<ShaderFileSystem> {
    public:
        ShaderFileSystem();
        ~ShaderFileSystem() override;

        void AddSearchPath(const FilePath &path) { searchPaths.emplace_back(path); }
        const std::vector<FilePath> &GetSearchPaths() const { return searchPaths; }

        void SetWorkFS(const FileSystemPtr& fs);
        void SetCacheFS(const FileSystemPtr& fs);
        void SetIntermediateFS(const NativeFileSystemPtr &fs);

        std::pair<bool, std::string> LoadCacheSource(const Name& name);
        void SaveCacheSource(const Name& name, const std::string &source);

        void AppendBinaryCache(const Name& name, ShaderCompileTarget target, const MD5 &md5, const CounterPtr<MemoryArchive>& memory);
        CounterPtr<MemoryArchive> LoadBinaryCache(const ShaderCacheEntry& entry, ShaderCompileTarget target);

        const NativeFileSystemPtr &GetIntermediateBinaryFS(ShaderCompileTarget target) const;
        const FileSystemPtr &GetCacheFS() const { return cacheFS; }

        void WaitSavingJobs();

    private:
        FileSystemPtr sourceFs;
        FileSystemPtr cacheFS;
        FileSystemPtr workFs;

        std::vector<FilePath> searchPaths;

        NativeFileSystemPtr cacheSourceFS;
        NativeFileSystemPtr compiledBinaryFS[static_cast<uint32_t>(ShaderCompileTarget::NUM)];

        tf::Executor executor;
    };

} // namespace sky
