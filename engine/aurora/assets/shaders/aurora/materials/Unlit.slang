#include <aurora/pipeline/GlobalBlock.slangh>
#include <aurora/pipeline/OpaquePass.slangh>
#include <aurora/pipeline/SceneBatch.slangh>
#include <aurora/pipeline/MaterialBatch.slangh>

float4 VSMain(float3 position : POSITION) : SV_Position
{
    return mul(gOpaquePass.viewProjection, mul(gSceneBatch.world, float4(position, 1)));
}

float4 FSMain() : SV_Target
{
    return gMaterial.tint;
}
