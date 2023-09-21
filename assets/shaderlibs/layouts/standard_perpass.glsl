layout (set = 0, binding = 0) uniform PassInfo {
    vec4 viewport;
} passInfo;

layout (set = 0, binding = 1) uniform ViewInfo {
    mat4 viewToWorldMatrix;
    mat4 worldToViewMatrix;
    mat4 viewToClipMatrix;
    mat4 worldToClipMatrix;
    mat4 clipToWorldMatrix;
    vec4 worldPos;
    vec4 zParam;
} viewInfo;
