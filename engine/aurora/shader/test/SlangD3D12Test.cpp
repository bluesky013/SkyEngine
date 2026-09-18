//
// Slang -> DXIL -> RHI shader object creation (DX12).
// NOTE: D3D12 root signature / ResourceGroup creation are implemented; this
// test still only validates shader object creation.
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

TEST_F(AuroraD3D12Test, SlangToRhiComputePipeline)
{
    RunSlangToRhiComputePipelineTest(GetDevice(), ShaderTarget::DXIL);
}

#endif // SKY_PLATFORM_WINDOWS
