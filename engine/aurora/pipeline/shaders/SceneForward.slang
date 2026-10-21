#include "ScenePass.h"
#include "SceneBatch.h"

float4 VSMain(float3 position : POSITION) : SV_Position
{
    return mul(gScenePass.viewProjection, mul(gSceneBatch.world, float4(position, 1)));
}

float4 FSMain() : SV_Target
{
    return gSceneBatch.tint;
}
