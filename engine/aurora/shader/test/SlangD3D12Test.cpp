//
// Slang -> DXIL -> RHI shader object creation (DX12).
// NOTE: D3D12 PipelineLayout/ResourceGroup creation is still a stub
// (aurora-resource-group DX12 backend); only shader objects are validated.
//

#include "SlangBackendTestCommon.h"

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

#if defined(SKY_PLATFORM_WINDOWS)

TEST_F(AuroraD3D12Test, SlangToRhiShaderObjects)
{
    RunSlangToRhiPipelineTest(GetDevice(), ShaderTarget::DXIL);
}

#endif // SKY_PLATFORM_WINDOWS
