#include <aurora/pipeline/SelectionMaskPass.slangh>
#include <aurora/pipeline/SceneBatch.slangh>

float4 VSMain(float3 position : POSITION) : SV_Position
{
    return mul(gSelectionMask.viewProjection, mul(gSceneBatch.world, float4(position, 1)));
}

float4 FSMain(float4 position : SV_Position) : SV_Target
{
    float depth = gSceneDepth.Load(int3(int2(position.xy), 0));
    float visible = position.z <= depth + 0.000002 ? 1.0 : 0.0;
    return float4(1, visible, 0, 1);
}
