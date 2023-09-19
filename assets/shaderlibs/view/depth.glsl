float ToLinearDepth(float z, vec4 zParam)
{
    return 1.0 / (zParam.z * z + zParam.w);
}