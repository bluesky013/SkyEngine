#define VIEW_COUNT 1

struct VSInput
{
    float4 Pos     : POSITION;
    float4 UV      : UV;
    float4 Normal  : NORMAL;
    float4 Tangent : TANGENT;
    float4 Color   : COLOR;

#if ENABLE_SKIN
    uint4 joints   : JOINTS;
    float4 weights : WEIGHTS;
#endif
};

struct VSOutput
{
    float4 Pos : SV_POSITION;
    float4 UV       : UV;
    float3 WorldPos : POSITION;
    float3 Normal   : NORMAL;
    float4 Tangent  : TANGENT;
    float4 Color    : COLOR;

#if VIEW_COUNT > 1
    nointerpolation uint ViewIndex : VIEWINDEX;
#endif
};