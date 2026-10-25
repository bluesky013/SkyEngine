//
// Created on 2026/10/22.
//

#include <editor/core/viewport/EditorCamera.h>

#include <algorithm>
#include <cmath>
#include <framework/serialization/PropertyCommon.h>
#include <framework/serialization/SerializationContext.h>

namespace sky::editor {
    void ViewportControlSettings::Normalize()
    {
        const auto bound = [](float value, float lo, float hi, float fallback) {
            return std::isfinite(value) ? std::clamp(value, lo, hi) : fallback;
        };
        speed           = bound(speed, .01f, 1000.f, 5.f);
        boost           = bound(boost, 1.f, 20.f, 4.f);
        sensitivity     = bound(sensitivity, .01f, 3.f, .25f);
        translationStep = bound(translationStep, .001f, 1000.f, .5f);
        rotationStep    = bound(rotationStep, .1f, 180.f, 15.f);
        scaleStep       = bound(scaleStep, .001f, 10.f, .1f);
    }

    void ViewportControlSettings::Reflect()
    {
        SerializationContext::Get()
            ->Register<ViewportControlSettings>("ViewportControlSettings")
            .Member<&ViewportControlSettings::speed>("speed")
            .Property(CommonPropertyKey::LABEL, Any(std::string("Camera speed")))
            .Property(CommonPropertyKey::ORDER, Any(0))
            .Property(CommonPropertyKey::RANGE_MIN, Any(.01))
            .Property(CommonPropertyKey::RANGE_MAX, Any(1000.))
            .Member<&ViewportControlSettings::boost>("boost")
            .Property(CommonPropertyKey::LABEL, Any(std::string("Shift multiplier")))
            .Property(CommonPropertyKey::ORDER, Any(1))
            .Property(CommonPropertyKey::RANGE_MIN, Any(1.))
            .Property(CommonPropertyKey::RANGE_MAX, Any(20.))
            .Member<&ViewportControlSettings::sensitivity>("sensitivity")
            .Property(CommonPropertyKey::LABEL, Any(std::string("Look (degrees/pixel)")))
            .Property(CommonPropertyKey::ORDER, Any(2))
            .Property(CommonPropertyKey::RANGE_MIN, Any(.01))
            .Property(CommonPropertyKey::RANGE_MAX, Any(3.))
            .Member<&ViewportControlSettings::translationStep>("translationStep")
            .Property(CommonPropertyKey::LABEL, Any(std::string("Move snap (units)")))
            .Property(CommonPropertyKey::ORDER, Any(3))
            .Property(CommonPropertyKey::RANGE_MIN, Any(.001))
            .Property(CommonPropertyKey::RANGE_MAX, Any(1000.))
            .Member<&ViewportControlSettings::rotationStep>("rotationStep")
            .Property(CommonPropertyKey::LABEL, Any(std::string("Rotate snap (degrees)")))
            .Property(CommonPropertyKey::ORDER, Any(4))
            .Property(CommonPropertyKey::RANGE_MIN, Any(.1))
            .Property(CommonPropertyKey::RANGE_MAX, Any(180.))
            .Member<&ViewportControlSettings::scaleStep>("scaleStep")
            .Property(CommonPropertyKey::LABEL, Any(std::string("Scale snap (ratio)")))
            .Property(CommonPropertyKey::ORDER, Any(5))
            .Property(CommonPropertyKey::RANGE_MIN, Any(.001))
            .Property(CommonPropertyKey::RANGE_MAX, Any(10.));
    }

    Vector3 EditorCamera::Forward() const
    {
        return {std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)};
    }
    Vector3 EditorCamera::Right() const
    {
        return {std::cos(yaw), 0, std::sin(yaw)};
    }
    Vector3 EditorCamera::Up() const
    {
        return Right().Cross(Forward());
    }
    Matrix4 EditorCamera::View() const
    {
        const auto r = Right(), u = Up(), f = Forward();
        return {{r.x, u.x, -f.x, 0}, {r.y, u.y, -f.y, 0}, {r.z, u.z, -f.z, 0}, {-r.Dot(position), -u.Dot(position), f.Dot(position), 1}};
    }
    Matrix4 EditorCamera::Projection(float aspect) const
    {
        const float cot   = 1.f / std::tan(verticalFov * .5f);
        const float depth = farPlane / (nearPlane - farPlane);
        return {{cot / std::max(aspect, .001f), 0, 0, 0}, {0, cot, 0, 0}, {0, 0, depth, -1}, {0, 0, nearPlane * depth, 0}};
    }
    bool EditorCamera::Project(const Vector3 &point, Vector2 extent, Vector2 &screen) const
    {
        const auto  delta = point - position;
        const float depth = delta.Dot(Forward());
        if (extent.x <= 0 || extent.y <= 0 || !std::isfinite(depth) || depth <= nearPlane)
            return false;
        const float pixels = extent.y / (2.f * std::tan(verticalFov * .5f) * depth);
        screen             = {extent.x * .5f + delta.Dot(Right()) * pixels, extent.y * .5f - delta.Dot(Up()) * pixels};
        return std::isfinite(screen.x) && std::isfinite(screen.y);
    }
    Vector3 EditorCamera::Ray(Vector2 screen, Vector2 extent) const
    {
        const float factor    = 2.f * std::tan(verticalFov * .5f) / std::max(extent.y, 1.f);
        auto        direction = Forward() + Right() * ((screen.x - extent.x * .5f) * factor) - Up() * ((screen.y - extent.y * .5f) * factor);
        direction.Normalize();
        return direction;
    }
    float EditorCamera::WorldUnitsPerPixel(const Vector3 &point, float height) const
    {
        return 2.f * std::tan(verticalFov * .5f) * std::max(nearPlane, (point - position).Dot(Forward())) / std::max(height, 1.f);
    }
    void EditorCamera::Look(float dx, float dy, float sensitivity, bool orbit)
    {
        yaw   = std::remainder(yaw + dx * sensitivity, 6.283185307f);
        pitch = std::clamp(pitch - dy * sensitivity, -1.553343f, 1.553343f);
        if (orbit)
            position = pivot - Forward() * orbitDistance;
        else
            pivot = position + Forward() * orbitDistance;
    }
    void EditorCamera::Pan(float dx, float dy, float height)
    {
        const auto delta = (Right() * -dx + Up() * dy) * WorldUnitsPerPixel(pivot, height);
        position += delta;
        pivot += delta;
    }
    void EditorCamera::Dolly(float distance)
    {
        const auto delta = Forward() * distance;
        position += delta;
        pivot += delta;
    }
    void EditorCamera::Fly(Vector3 input, float seconds, float speed)
    {
        if (!std::isfinite(seconds) || !std::isfinite(speed) || seconds <= 0 || speed <= 0)
            return;
        auto direction = Right() * input.x + Vector3{0, 1, 0} * input.y + Forward() * input.z;
        if (direction.Length() < 1.e-6f)
            return;
        direction.Normalize();
        const auto delta = direction * (std::min(input.Length(), 1.f) * seconds * speed);
        position += delta;
        pivot += delta;
    }
    void EditorCamera::Focus(const Vector3 &target)
    {
        pivot         = target;
        orbitDistance = std::max(nearPlane * 2.f, orbitDistance);
        position      = pivot - Forward() * orbitDistance;
    }
} // namespace sky::editor
