//
// Created by blues on 2024/12/30.
//

#include <shader/ShaderFileSystem.h>
#include <shader/ShaderCompiler.h>
#include <shader/ShaderCacheManager.h>

namespace sky {

    ShaderFileSystem::ShaderFileSystem() : executor(1)
    {
    }

    ShaderFileSystem::~ShaderFileSystem()
    {
        executor.wait_for_all();
    }

    void ShaderFileSystem::SetWorkFS(const FileSystemPtr& fs)
    {
        cacheFS = fs;
    }

    void ShaderFileSystem::SetIntermediateFS(const NativeFileSystemPtr &fs)
    {
        cacheSourceFS = CastPtr<NativeFileSystem>(fs->CreateSubSystem("Sources", true));

        for (uint32_t i = 0; i < static_cast<uint32_t>(ShaderCompileTarget::NUM); ++i) {
            compiledBinaryFS[i] = CastPtr<NativeFileSystem>(fs->CreateSubSystem(ShaderCompiler::GetTargetName(static_cast<ShaderCompileTarget>(i)).GetStr().data(), true));
        }
    }

    const NativeFileSystemPtr &ShaderFileSystem::GetIntermediateBinaryFS(ShaderCompileTarget target) const
    {
        return compiledBinaryFS[static_cast<uint32_t>(target)];
    }

    void ShaderFileSystem::WaitSavingJobs()
    {
        executor.wait_for_all();
    }

    std::pair<bool, std::string> ShaderFileSystem::LoadCacheSource(const Name& name)
    {
        if (cacheSourceFS) {
            auto replacedName = ShaderCompiler::ReplaceShadeName(name);
            auto file = cacheSourceFS->OpenFile(FilePath(replacedName));

            if (file) {
                std::string result;
                file->ReadString(result);
                return {true, result};
            }
        }

        return {false, {}};
    }

    void ShaderFileSystem::SaveCacheSource(const Name& name, const std::string &source)
    {
        auto replacedName = ShaderCompiler::ReplaceShadeName(name);
        if (cacheSourceFS) {
            executor.silent_async([fs = cacheSourceFS, source, replacedName]() {
                auto file = fs->CreateOrOpenFile(replacedName);
                auto archive = file->WriteAsArchive();
                archive->SaveRaw(source.c_str(), source.length());
            });
        }
    }

    CounterPtr<MemoryArchive> ShaderFileSystem::LoadBinaryCache(const ShaderCacheEntry& entry, ShaderCompileTarget target)
    {
        if (!cacheFS) {
            return {};
        }

        FilePath filePath(entry.savedFileName.GetStr());
        FilePath targetPath(ShaderCompiler::GetTargetName(target).GetStr().data());

        auto file = cacheFS->OpenFile(targetPath / filePath);

        CounterPtr<MemoryArchive> memory = new MemoryArchive();
        memory->Resize(entry.length);

        file->ReadData(entry.offset, entry.length, reinterpret_cast<uint8_t *>(memory->Address()));

        return memory;
    }

    void ShaderFileSystem::AppendBinaryCache(const Name& shader, ShaderCompileTarget target, const MD5 &md5, const CounterPtr<MemoryArchive>& memory)
    {
        if (!cacheFS) {
            return;
        }

        FilePath filePath(ShaderCompiler::ReplaceShadeName(shader));
        filePath.ReplaceExtension(".bin");

        executor.silent_async([filePath, target, memory, md5]() {
            auto fs = ShaderFileSystem::Get()->GetCacheFS();
            SKY_ASSERT(!fs->IsReadOnly())


            auto subFS = fs->CreateSubSystem(ShaderCompiler::GetTargetName(target).GetStr().data(), true);
            auto file = subFS->CreateOrOpenFile(filePath);

            ShaderCacheHeader header = {};
            file->ReadData(0, sizeof(ShaderCacheHeader), reinterpret_cast<uint8_t *>(&header));
            if (header.magic != SHADER_CACHE_MAGIC) {
                // recreate file
                header.magic = SHADER_CACHE_MAGIC;
                auto archive = file->WriteAsArchive();
                archive->SaveRaw(reinterpret_cast<const char *>(&header), sizeof(ShaderCacheHeader));
            }

            ShaderCacheEntry cacheEntry = {};
            cacheEntry.savedFileName = Name(filePath.GetStr().c_str());
            cacheEntry.offset = static_cast<uint32_t>(file->AppendData(memory->Data(), memory->Size()));
            cacheEntry.length = static_cast<uint32_t>(memory->Size());

            Event<IShaderCacheEvent>::BroadCast(&IShaderCacheEvent::OnShaderCacheSaved, target, md5, cacheEntry);
        });
    }
} // namespace sky