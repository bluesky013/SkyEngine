#include "shaders/vertex/full_screen.hlsl"

struct VSOutput
{
    float4 Pos : SV_POSITION;
};

VSOutput VSMain(uint vid : SV_VertexID)
{
    VSOutput res;
    res.Pos = float4(FT_POSITIONS[vid], 0.0, 1.0);
    return res;
}

float4 FSMain(VSOutput input) : SV_TARGET
{
    return float4(0, 0, 0, 0);
}