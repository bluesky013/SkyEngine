#include "shaders/vertex/full_screen.hlsl"

#include "shaders/lighting/pbr.hlsl"

float2 BRDF_SAMPLE(float NoV, float roughness)
{
    const uint NUM_SAMPLES = 1024;

	// Normal always points along z-axis for the 2D lookup
	const float3 N = float3(0.0, 0.0, 1.0);
	float3 V = float3(sqrt(1.0 - NoV * NoV), 0.0, NoV);

	float2 LUT = float2(0.0, 0.0);
	for(uint i = 0u; i < NUM_SAMPLES; i++) {
		float2 Xi = Hammersley2d(i, NUM_SAMPLES);
		float3 H = ImportanceSample_GGX(Xi, roughness, N);
		float3 L = 2.0 * dot(V, H) * H - V;

		float dotNL = max(dot(N, L), 0.0);
		float dotNV = max(dot(N, V), 0.0);
		float dotVH = max(dot(V, H), 0.0);
		float dotNH = max(dot(H, N), 0.0);

		if (dotNL > 0.0) {
			float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
			float G_Vis = (G * dotVH) / (dotNH * dotNV);
			float Fc = pow(1.0 - dotVH, 5.0);
			LUT += float2((1.0 - Fc) * G_Vis, Fc * G_Vis);
		}
	}
	return LUT / float(NUM_SAMPLES);
}

float4 FSMain(VSOutput input) : SV_TARGET
{
    return float4(BRDF_SAMPLE(input.UV.x, input.UV.y), 0.0, 1.0);
}