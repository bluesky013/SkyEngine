#include <aurora/pipeline/ShadowPass.slangh>
#include <aurora/pipeline/SceneBatch.slangh>

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
    return mul(gShadowPass.lightViewProjection, mul(gSceneBatch.world, float4(position, 1)));
}
void FSMain() {}
