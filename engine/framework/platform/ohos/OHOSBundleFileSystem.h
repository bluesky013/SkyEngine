#pragma once

#include <core/file/FileSystem.h>
#include <memory>

struct NativeResourceManager;

namespace sky {

    FileSystemPtr CreateOHOSBundleFileSystem(std::shared_ptr<NativeResourceManager> manager);

} // namespace sky
