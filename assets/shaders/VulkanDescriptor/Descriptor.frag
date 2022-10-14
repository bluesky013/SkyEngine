#version 450

layout (location = 0) in vec2 inUv;

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 0) uniform sampler   sampler0;
layout (set = 0, binding = 1) uniform texture2D texture0;
layout (set = 0, binding = 2) uniform sampler2D imageSampler0;
layout (set = 0, binding = 3, rgba8) uniform writeonly image2D  storageImage;

layout (set = 0, binding = 4) uniform Ubo{
    vec2 position;
    vec2 scale;
} ubo;

layout (set = 0, binding = 5) uniform Ext{
    vec2 value;
} ext;

layout (set = 0, binding = 6) uniform textureBuffer texelBuffer0;
layout (set = 0, binding = 7, rgba8) uniform writeonly imageBuffer texelBuffer1;

void main()
{
    vec2 coord = gl_FragCoord.xy;
    vec2 halfExt = ext.value / vec2(2.0);

    vec2 lt = ubo.position - halfExt;
    vec2 rb = ubo.position + halfExt;

    int texCoord = int(gl_FragCoord.x / ubo.scale.x) % 2;
    vec4 texelColor = texelFetch(texelBuffer0, texCoord);
    imageStore(texelBuffer1, texCoord, texelColor / vec4(2.0));

    if (gl_FragCoord.x >= lt.x && gl_FragCoord.y >= lt.y && gl_FragCoord.x <= rb.x && gl_FragCoord.y <= rb.y) {
        vec2 diff = gl_FragCoord.xy - lt;
        vec2 uv = vec2(diff) / ext.value;
        imageStore(storageImage, ivec2(diff), texture(sampler2D(texture0, sampler0), inUv));
        outFragColor = texture(imageSampler0, uv);
    } else {
        outFragColor = texture(sampler2D(texture0, sampler0), inUv) * texelColor;
    }
}
