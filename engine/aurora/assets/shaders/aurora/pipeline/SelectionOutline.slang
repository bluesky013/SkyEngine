#include <aurora/pipeline/SelectionOutlinePass.slangh>

float4 VSMain(uint id : SV_VertexID) : SV_Position
{
    float2 p = float2((id << 1) & 2, id & 2);
    return float4(p * 2 - 1, 0, 1);
}

float4 FSMain(float4 position : SV_Position) : SV_Target
{
    int2 pixel = int2(position.xy);
    float2 center = gSelectionImage.Load(int3(pixel, 0)).rg;
    if (center.r > 0.5)
        return float4(0, 0, 0, 0);
    int radius = int(ceil(gSelectionOutline.metrics.z));
    float2 coverage = float2(0, 0);
    for (int y = -radius; y <= radius; ++y)
        for (int x = -radius; x <= radius; ++x) {
            if (x * x + y * y > gSelectionOutline.metrics.z * gSelectionOutline.metrics.z)
                continue;
            int2 at = pixel + int2(x, y);
            if (any(at < 0) || any(at >= int2(gSelectionOutline.metrics.xy)))
                continue;
            coverage = max(coverage, gSelectionImage.Load(int3(at, 0)).rg);
        }
    if (coverage.r < 0.5)
        return float4(0, 0, 0, 0);
    return coverage.g > 0.5 ? gSelectionOutline.visible : gSelectionOutline.occluded;
}
