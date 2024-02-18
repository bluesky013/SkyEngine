struct LightInfo {
    float4 Direction; // xyz: dir
    float4 Color;     // xyz: rgb w: intensity
};

struct StandardPBR {
    float Metallic;
    float Roughness;
    float3 Albedo;
};