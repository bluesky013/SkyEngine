//
// RgBlockDesc CPU field layout.
//

#include "AuroraTestHelper.h"

#include <aurora/shader/RgBlockDesc.h>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

TEST_F(AuroraVulkanTest, RgBlockDescFieldOffsets)
{
    RgBlockDesc desc{};
    desc.fields = {
        {RgFieldType::MAT4, Name("World")},      // 64B at offset 0
        {RgFieldType::FLOAT, Name("Intensity")}, // 4B at offset 64
        {RgFieldType::FLOAT3, Name("Color")},    // vec3: 16B aligned -> offset 80
    };

    uint32_t   total   = 0;
    const auto layouts = ComputeFieldOffsets(desc, total);

    ASSERT_EQ(layouts.size(), 3u);
    EXPECT_EQ(layouts[0].offset, 0u);
    EXPECT_EQ(layouts[0].size, 64u);
    EXPECT_EQ(layouts[1].offset, 64u);
    EXPECT_EQ(layouts[1].size, 4u);
    EXPECT_EQ(layouts[2].offset, 80u); // 16B aligned, not 68
    EXPECT_EQ(layouts[2].size, 16u);
    EXPECT_EQ(total, 96u);
}
