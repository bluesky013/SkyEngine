//
// Slang -> MSL -> RHI shader object creation (Metal).
// Compiles to nothing on non-Apple platforms.
//

#include "SlangBackendTestCommon.h"

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

#if defined(SKY_PLATFORM_MACOS) || defined(SKY_PLATFORM_IOS)

TEST_F(AuroraMetalTest, SlangToRhiShaderObjects)
{
    RunSlangToRhiPipelineTest(GetDevice(), ShaderTarget::MSL);
}

#endif // SKY_PLATFORM_MACOS || SKY_PLATFORM_IOS
