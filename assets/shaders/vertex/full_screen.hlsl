#define VIEW_COUNT 1

struct VSOutput
{
    float4 Pos : SV_POSITION;
    [[vk::location(0)]] float2 UV  : TEXCOORD;
#if VIEW_COUNT > 1
    [[vk::location(1)]] nointerpolation uint ViewIndex : VIEWINDEX;
#endif
};

VSOutput VSMain(uint vid : SV_VertexID
#if VIEW_COUNT > 1
    , uint ViewIndex : SV_ViewID
#endif
)
{
    const float2 FT_POSITIONS[3] = {
        float2(-1.0,  3.0),
        float2(-1.0, -1.0),
        float2( 3.0, -1.0)
    };

    const float2 FT_UVS[3] = {
        float2(0.0, 2.0),
        float2(0.0, 0.0),
        float2(2.0, 0.0)
    };

    VSOutput res;

    res.Pos = float4(FT_POSITIONS[vid], 0.0, 1.0);
    res.UV = FT_UVS[vid];

#if VIEW_COUNT > 1
    res.ViewIndex = ViewIndex;
#endif
    return res;
}