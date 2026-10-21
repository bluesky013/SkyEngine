#pragma once

#include <aurora/shader/ShaderFileSystem.h>
#include <gtest/gtest.h>

namespace sky::aurora {
    inline bool LoadTestShaderSources(ShaderFileSystem &files)
    {
        NativeFileSystem root{FilePath(SHADER_TEST_SOURCE_ROOT)};
        std::string      error;
        const bool       loaded = files.LoadBundles(root, "", &error);
        EXPECT_TRUE(loaded) << error;
        return loaded;
    }
    inline std::shared_ptr<ShaderFileSystem> TestShaderSources()
    {
        auto files = std::make_shared<ShaderFileSystem>();
        return LoadTestShaderSources(*files) ? files : nullptr;
    }
} // namespace sky::aurora
