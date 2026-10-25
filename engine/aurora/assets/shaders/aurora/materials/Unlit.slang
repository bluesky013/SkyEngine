#include <aurora/pipeline/GlobalBlock.slangh>
#include <aurora/pipeline/OpaquePass.slangh>
#include <aurora/pipeline/SceneBatch.slangh>
#include <aurora/pipeline/MaterialBlock.slangh>

#if SKINNED
#include <aurora/materials/Skinning.slangh>
float4 VSMain(float3 position : POSITION, uint4 joints : JOINTS, float4 weights : WEIGHTS) : SV_Position
#else
float4 VSMain(float3 position : POSITION) : SV_Position
#endif
{
#if SKINNED
    position = SkinPosition(position, joints, weights);
#endif
    return mul(gOpaquePass.viewProjection, mul(gSceneBatch.world, float4(position, 1)));
}

float4 FSMain() : SV_Target
{
    return gMaterial.tint;
}
