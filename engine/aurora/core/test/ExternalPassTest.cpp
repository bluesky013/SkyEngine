#include "plugins/first/shaders/ToonPass.h"
#include "plugins/second/shaders/ToonPass.h"
#include <aurora/rdg/FrameBindings.h>
#include <aurora/renderer/PassResourceBindings.h>
#include <gtest/gtest.h>

using namespace sky::aurora;

namespace {
    class ExternalShader final : public Shader {
    public:
        explicit ExternalShader(const ShaderReflection &value) : reflection(value)
        {
        }
        const ShaderReflection &GetReflection() const override
        {
            return reflection;
        }

    private:
        ShaderReflection reflection;
    };
} // namespace

TEST(ExternalPass, IndependentLayoutsAndNamesLinkWithoutBuiltinPipeline)
{
    static_assert(!std::is_same_v<test_pipeline::first::ToonParams, test_pipeline::second::ToonParams>);
    EXPECT_EQ(sizeof(test_pipeline::first::ToonParams), 16u);
    EXPECT_EQ(sizeof(test_pipeline::second::ToonParams), 80u);
    for (const auto *schema : {&test_pipeline::first::GetToonPassReflection(), &test_pipeline::second::GetToonPassReflection()}) {
        RasterDraw draw;
        draw.shader = new ExternalShader(*schema);
        PassResourceBindings bindings(*schema);
        for (const auto &block : schema->blocks) {
            ASSERT_TRUE(
                std::holds_alternative<FrameOperationComplete>(bindings.Uniform({block.set, block.binding}, {{1, block.set}, 0, block.size})));
        }
        const PassResourceBindings *groups[]{&bindings};
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(PassResourceBindings::Apply(draw, groups)));
        ASSERT_EQ(ResolveRasterBindings(draw).uniforms.size(), 3u);
        EXPECT_EQ(ResolveRasterBindings(draw).uniforms[1].binding, schema->blocks[1].binding);
    }
}
