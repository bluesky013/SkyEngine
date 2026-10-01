//
// Aurora light system implementation.
//

#include <aurora/light/LightSystem.h>
#include <aurora/light/LightTypes.h>
#include <aurora/scene/RenderScene.h>
#include <aurora/scene/SceneTypes.h>

#include <core/ecs/EntityId.h>

namespace sky::aurora {

    Vector3 ExtractLightPosition(const Matrix4 &world)
    {
        return Vector3(world.m[3].x, world.m[3].y, world.m[3].z);
    }

    Vector3 ExtractLightDirection(const Matrix4 &world)
    {
        const Vector4 dir = world * Vector4(VEC3_NZ.x, VEC3_NZ.y, VEC3_NZ.z, 0.f);

        Vector3 result(dir.x, dir.y, dir.z);
        if (result.Dot(result) > 1e-12f) {
            result.Normalize();
        } else {
            result = VEC3_NZ;
        }
        return result;
    }

    void LocalLightSystem::Process(RenderScene &scene, std::vector<LightRenderData> &out)
    {
        auto &reg  = scene.GetRegistry();
        auto  view = reg.View<Light, WorldInfo>();

        view.ForEach([&](EntityId, const Light &light, const WorldInfo &worldInfo) {
            const Vector3 position  = ExtractLightPosition(worldInfo.world);
            const Vector3 direction = ExtractLightDirection(worldInfo.world);

            LightRenderData data{};
            data.position  = Vector4(position.x, position.y, position.z, static_cast<float>(light.type));
            data.color     = Vector4(light.color.x, light.color.y, light.color.z, light.intensity);
            data.direction = Vector4(direction.x, direction.y, direction.z, 0.f);
            data.params    = Vector4(light.range, light.innerConeAngle, light.outerConeAngle, 0.f);

            out.push_back(data);
        });
    }

} // namespace sky::aurora
