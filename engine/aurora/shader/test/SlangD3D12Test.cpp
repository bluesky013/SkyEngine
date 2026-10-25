// Slang DXIL objects, pipelines and stage-specific variant readback.

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

TEST_F(AuroraD3D12Test, StageVariantValuesRemainIndependentOnGpu)
{
    RunStageVariantReadback(GetDevice(), ShaderTarget::DXIL);
}

#endif // SKY_PLATFORM_WINDOWS
