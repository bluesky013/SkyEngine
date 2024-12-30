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

    class ShaderFileSystem : public Singleton<ShaderFileSystem> {
    public:
        ShaderFileSystem();
        ~ShaderFileSystem() override;

        void SetSourceFS(const FileSystemPtr& fs);
        void SetWorkFS(const FileSystemPtr& fs);
        void SetIntermediateFS(const NativeFileSystemPtr &fs);

        void SaveCacheSource(const Name& name, const std::string &source);
        void AppendBinaryCache(const Name& name, ShaderCompileTarget target, const MD5 &md5, const CounterPtr<MemoryArchive>& memory);

        const NativeFileSystemPtr &GetIntermediateBinaryFS(ShaderCompileTarget target) const;
        const FileSystemPtr &GetCacheFS() const { return cacheFS; }

        void WaitSavingJobs();

    private:
        FileSystemPtr sourceFs;
        FileSystemPtr cacheFS;

        NativeFileSystemPtr cacheSourceFS;
        NativeFileSystemPtr compiledBinaryFS[static_cast<uint32_t>(ShaderCompileTarget::NUM)];

        tf::Executor executor;
    };

} // namespace sky
