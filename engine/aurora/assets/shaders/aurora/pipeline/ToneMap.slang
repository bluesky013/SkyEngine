#include <aurora/pipeline/ToneMapPass.slangh>

float4 VSMain(uint id : SV_VertexID) : SV_Position
{
    float2 p = float2((id << 1) & 2, id & 2);
    return float4(p * 2 - 1, 0, 1);
}

float4 FSMain(float4 position : SV_Position) : SV_Target
{
    float3 color = max(gHdrColor.SampleLevel(gHdrSampler, position.xy * gToneMap.display.xy, 0).rgb, 0);
    color *= exp2(gToneMap.display.z);
    color = color / (1 + color);
    if (gToneMap.display.w > 0)
        color = select(color <= .0031308, color * 12.92, 1.055 * pow(color, 1 / 2.4) - .055);
    return float4(color, 1);
}
