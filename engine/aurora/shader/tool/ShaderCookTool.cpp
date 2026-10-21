//
// ShaderCookTool: offline shader precompilation.
//
// Reads a `shader_usage.json` (produced by runtime collection), resolves
// schemas + compiles each used (path, entry, stage, target, variant), and writes
// `index.bin` / `blobs` into the local cache root so the shipped build can run
// without the compiler module.
//
// Usage:
//   ShaderCookTool <usage.json> <sourceRoot> <cacheDir> [offlineCacheDir]
//

#include <aurora/shader/ShaderCompilerSlang.h>
#include <aurora/shader/ShaderFileSystem.h>
#include <aurora/shader/ShaderResolver.h>
#include <aurora/shader/ShaderUsage.h>

#include <core/file/FileSystem.h>

#include <filesystem>
#include <iostream>
#include <string>

using namespace sky;
using namespace sky::aurora;

int main(int argc, char **argv)
{
    if (argc < 4 || argc > 5) {
        std::cerr << "usage: ShaderCookTool <usage.json> <sourceRoot> <cacheDir> [offlineCacheDir]\n";
        return 1;
    }

    const std::string usagePath = argv[1];
    const std::string sourceDir = argv[2];
    const std::string cacheDir  = argv[3];

    // usage file
    FileSystemPtr     usageFs   = FileSystemPtr(new NativeFileSystem(FilePath(std::filesystem::path(usagePath).parent_path().string())));
    const std::string usageName = std::filesystem::path(usagePath).filename().string();

    std::vector<ShaderUsageEntry> usage;
    if (!ShaderUsageCollector::Load(*usageFs, usageName, usage)) {
        std::cerr << "failed to load usage: " << usagePath << "\n";
        return 1;
    }
    std::cout << "usage entries: " << usage.size() << "\n";

    ShaderFileSystem source;
    NativeFileSystem sourceRoot{FilePath(sourceDir)};
    std::string      error;
    if (!source.LoadBundles(sourceRoot, "", &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    // local (writable) cache root
    std::error_code directoryError;
    std::filesystem::create_directories(cacheDir, directoryError);
    if (directoryError) {
        std::cerr << "cache directory unavailable: " << directoryError.message() << "\n";
        return 1;
    }
    FileSystemPtr local = new NativeFileSystem(FilePath(cacheDir));

    // optional read-only offline root
    FileSystemPtr offline;
    if (argc == 5) {
        offline = new NativeFileSystem(FilePath(std::string(argv[4])));
    }

    ShaderCompilerSlang compiler;
    ShaderCompilerFactory::Get().Register(&compiler);

    ShaderResolver resolver(offline.Get(), local.Get(), source, 0, 0);

    const bool ok = BuildUsageCache(resolver, usage, &error);
    if (!ok) {
        std::cerr << "cook failed: " << error << "\n";
        return 1;
    }

    ShaderFileSystem emptySources;
    ShaderResolver   published(local.Get(), nullptr, emptySources, 0, 0);
    if (!BuildUsageCache(published, usage, &error)) {
        std::cerr << "published shader products are incomplete: " << error << "\n";
        return 1;
    }

    std::cout << "cooked " << usage.size() << " shader entries into " << cacheDir << "\n";
    return 0;
}
