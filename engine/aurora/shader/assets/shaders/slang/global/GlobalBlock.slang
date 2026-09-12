// Global render block: single source of truth for the set-0 global UBO.
// ShaderHeaderTool reflects this file and emits the C++ mirror header.

struct GlobalParams {
    float4x4 view;
    float4x4 proj;
    float4x4 viewProj;
    float4 cameraPos;
};

[[vk::binding(0, 0)]] ParameterBlock<GlobalParams> gGlobal;
