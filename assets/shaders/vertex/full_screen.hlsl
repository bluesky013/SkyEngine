struct VSOutput
{
    float4 Pos : SV_POSITION;

    [[vk::location(0)]] float2 UV   : TEXCOORD;
};

VSOutput VSMain(uint vid : SV_VertexID)
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
    return res;
}