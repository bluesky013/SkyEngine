// Compile the renderer's actual shader source without creating a GPU device.

#include "UIShader.h"

#include <aurora/shader/ShaderCompilerSlang.h>
#include <core/platform/Platform.h>
#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <string_view>

namespace sky::ui {

    namespace {

        struct UIShaderEntry {
            const char                *name;
            aurora::ShaderStageFlagBit stage;
        };

        class UIShaderTest : public testing::TestWithParam<UIShaderEntry> {
        protected:
            void ExpectCompiles(aurora::ShaderTarget target)
            {
                SCOPED_TRACE(GetParam().name);
                aurora::ShaderCompilerSlang compiler;
                aurora::ShaderCompileDesc   request{};
                request.source     = detail::UI_SHADER_SOURCE;
                request.sourcePath = "ui/UIShader.slang";
                request.entry      = GetParam().name;
                request.stage      = GetParam().stage;
                request.target     = target;

                aurora::ShaderCompileResult result;
                ASSERT_TRUE(compiler.Compile(request, result)) << result.errorInfo;
                ASSERT_FALSE(result.data.empty());
                std::string error;
                EXPECT_TRUE(aurora::ValidateShaderArtifact(result, error)) << error;
                EXPECT_EQ(result.reflection.resources.size(), std::string_view(GetParam().name) == "fs_main" ? 2u : 0u);
                EXPECT_EQ(result.reflection.declarations.size(), 2u);
                for (const auto &resource : result.reflection.declarations)
                    EXPECT_EQ(resource.set, 3u);
                if (target == aurora::ShaderTarget::SPIRV) {
                    ASSERT_GE(result.data.size(), sizeof(uint32_t));
                    uint32_t magic = 0;
                    std::memcpy(&magic, result.data.data(), sizeof(magic));
                    EXPECT_EQ(magic, 0x07230203u);
                } else if (target == aurora::ShaderTarget::MSL) {
                    const std::string source(reinterpret_cast<const char *>(result.data.data()), result.data.size());
                    EXPECT_NE(source.find("metal_stdlib"), std::string::npos);
                } else {
                    ASSERT_GE(result.data.size(), 4u);
                    const char *bytes = reinterpret_cast<const char *>(result.data.data());
                    EXPECT_EQ(std::string(bytes, 4), "DXBC");
                }
            }
        };

        TEST_P(UIShaderTest, CompilesSpirv)
        {
            ExpectCompiles(aurora::ShaderTarget::SPIRV);
        }

        TEST_P(UIShaderTest, CompilesMsl)
        {
            ExpectCompiles(aurora::ShaderTarget::MSL);
        }

        TEST_P(UIShaderTest, CompilesDxil)
        {
#if defined(SKY_PLATFORM_WINDOWS)
            ExpectCompiles(aurora::ShaderTarget::DXIL);
#else
            GTEST_SKIP() << "DXIL compilation requires the configured Win32 dxcompiler package";
#endif
        }

        INSTANTIATE_TEST_SUITE_P(Entries,
                                 UIShaderTest,
                                 testing::Values(UIShaderEntry{"vs_main", aurora::ShaderStageFlagBit::VS},
                                                 UIShaderEntry{"fs_main", aurora::ShaderStageFlagBit::FS},
                                                 UIShaderEntry{"fs_round", aurora::ShaderStageFlagBit::FS}),
                                 [](const testing::TestParamInfo<UIShaderEntry> &info) { return info.param.name; });

    } // namespace

} // namespace sky::ui
