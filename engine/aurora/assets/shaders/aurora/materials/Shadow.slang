#include <aurora/pipeline/ShadowPass.slangh>
#include <aurora/pipeline/SceneBatch.slangh>

float4 VSMain(float3 position : POSITION) : SV_Position
{
    return mul(gShadowPass.lightViewProjection, mul(gSceneBatch.world, float4(position, 1)));
}
void FSMain() {}
