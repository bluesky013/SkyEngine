//
// Slang -> SPIRV -> RHI shader object creation (Vulkan).
//

#include "SlangBackendTestCommon.h"

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

TEST_F(AuroraVulkanTest, SlangToRhiShaderObjects)
{
    RunSlangToRhiPipelineTest(GetDevice(), ShaderTarget::SPIRV);
}
