struct Box {
    vec3 center;
    vec3 extent;
};

struct Ray {
    vec3 origin;
    vec3 dir;
};

vec2 rayBoxDist(Box box, Ray ray)
{
    vec3 boundsMin = box.center - box.extent / 2;
    vec3 boundsMax = box.center + box.extent / 2;

    vec3 t0 = (boundsMin - ray.origin) / ray.dir;
    vec3 t1 = (boundsMax - ray.origin) / ray.dir;
    vec3 tmin = min(t0, t1);
    vec3 tmax = max(t0, t1);

    float dst1 = max(max(tmin.x, tmin.y), tmin.z);
    float dst2 = min(tmax.x, min(tmax.y, tmax.z));
    float dstToBox = max(0, dst1);
    float dstInsideBox = max(0, dst2 - dstToBox);

    return vec2(dstToBox, dstInsideBox);
}