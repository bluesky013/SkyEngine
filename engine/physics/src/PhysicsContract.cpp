//
// Created on 2026/09/23.
//

#include <physics/PhysicsBody.h>
#include <physics/PhysicsCharacter.h>
#include <physics/PhysicsConstraint.h>
#include <physics/PhysicsFilter.h>
#include <physics/PhysicsShapes.h>
#include <physics/PhysicsStepping.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace sky::phy {

    namespace {

        bool Fail(std::string *why, const char *msg)
        {
            if (why != nullptr) {
                *why = msg;
            }
            return false;
        }

        bool Finite(const Vector3 &value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }

        bool ValidateShapeImpl(const ShapeDesc &desc, std::string *why, uint32_t depth = 0)
        {
            if (depth > 32 || !Finite(desc.pivot))
                return Fail(why, "invalid shape transform or compound depth");
            switch (desc.type) {
            case ShapeType::Box:
                if (!Finite(desc.halfExt) || desc.halfExt.x <= 0.f || desc.halfExt.y <= 0.f || desc.halfExt.z <= 0.f) {
                    return Fail(why, "box half extent must be positive");
                }
                break;
            case ShapeType::Sphere:
                if (!std::isfinite(desc.radius) || desc.radius <= 0.f) {
                    return Fail(why, "sphere radius must be positive");
                }
                break;
            case ShapeType::Capsule:
                if (!std::isfinite(desc.radius) || desc.radius <= 0.f) {
                    return Fail(why, "capsule radius must be positive");
                }
                if (!std::isfinite(desc.height) || desc.height < 0.f) {
                    return Fail(why, "capsule height must be non-negative");
                }
                break;
            case ShapeType::HeightField:
                if (desc.cols < 2 || desc.rows < 2) {
                    return Fail(why, "heightfield needs at least a 2x2 grid");
                }
                if (desc.samples.size() != static_cast<size_t>(desc.cols) * desc.rows) {
                    return Fail(why, "heightfield sample count does not match cols * rows");
                }
                if (!std::isfinite(desc.scaleX) || !std::isfinite(desc.scaleZ) || !std::isfinite(desc.heightScale) ||
                    !std::isfinite(desc.heightOffset) || desc.scaleX <= 0.f || desc.scaleZ <= 0.f || desc.heightScale == 0.f ||
                    std::any_of(desc.samples.begin(), desc.samples.end(), [](float sample) { return !std::isfinite(sample); })) {
                    return Fail(why, "heightfield scales must be non-zero");
                }
                if (desc.upAxis > 2) {
                    return Fail(why, "heightfield up axis must be 0, 1 or 2");
                }
                break;
            case ShapeType::TriangleMesh:
                if (!desc.meshData && !static_cast<bool>(desc.mesh)) {
                    return Fail(why, "triangle mesh needs cooked data or a mesh asset");
                }
                break;
            case ShapeType::ConvexHull:
                if (desc.points.size() < 4 || std::any_of(desc.points.begin(), desc.points.end(), [](const auto &point) { return !Finite(point); })) {
                    return Fail(why, "convex hull needs at least 4 points");
                }
                break;
            case ShapeType::Compound:
                if (desc.children.empty()) {
                    return Fail(why, "compound shape needs at least one child");
                }
                for (const auto &child : desc.children) {
                    if (!ValidateShapeImpl(child, why, depth + 1)) {
                        return false;
                    }
                }
                break;
            default: return Fail(why, "unknown shape type");
            }
            return true;
        }

    } // namespace

    bool ValidateShape(const ShapeDesc &desc, std::string *why)
    {
        return ValidateShapeImpl(desc, why);
    }

    bool ValidateBodyDesc(const PhysicsBodyDesc &desc, std::string *why)
    {
        if (!ValidateShapeImpl(desc.shape, why)) {
            return false;
        }
        if (desc.kind > BodyKind::Kinematic || !std::isfinite(desc.mass) || desc.mass <= 0.f) {
            return Fail(why, "body kind and mass must be valid");
        }
        const auto &transform = desc.transform;
        const auto &rotation  = transform.rotation;
        const float length    = rotation.x * rotation.x + rotation.y * rotation.y + rotation.z * rotation.z + rotation.w * rotation.w;
        if (!Finite(transform.translation) || !Finite(transform.scale) || transform.scale.x <= 0 || transform.scale.y <= 0 ||
            transform.scale.z <= 0 || !std::isfinite(length) || length <= 0)
            return Fail(why, "body transform must be finite with positive scale and a nonzero rotation");
        if (!Finite(desc.inertia) || desc.inertia.x < 0 || desc.inertia.y < 0 || desc.inertia.z < 0)
            return Fail(why, "invalid body inertia");
        if (!Finite(desc.linearDamping) || desc.linearDamping.x < 0.f || desc.linearDamping.y < 0.f || desc.linearDamping.z < 0.f) {
            return Fail(why, "linear damping must be non-negative");
        }
        if (!Finite(desc.angularDamping) || desc.angularDamping.x < 0.f || desc.angularDamping.y < 0.f || desc.angularDamping.z < 0.f) {
            return Fail(why, "angular damping must be non-negative");
        }
        return true;
    }

    bool ValidateConstraintDesc(const ConstraintDesc &desc, std::string *why)
    {
        const bool hasA = IsValid(desc.bodyA);
        const bool hasB = IsValid(desc.bodyB);
        if (!hasA && !hasB) {
            return Fail(why, "constraint needs at least one valid body");
        }
        if (desc.type == ConstraintType::Distance && desc.distance < 0.f) {
            return Fail(why, "distance constraint distance must be non-negative");
        }
        if (desc.type == ConstraintType::Hinge || desc.type == ConstraintType::Slider) {
            if (desc.axisA.Length() <= 0.f) {
                return Fail(why, "constraint axis must be non-zero");
            }
        }
        const auto validLimit = [](const ConstraintLimit &limit) { return !limit.limited || limit.lower <= limit.upper; };
        if (!validLimit(desc.linearX) || !validLimit(desc.linearY) || !validLimit(desc.linearZ) || !validLimit(desc.angularX) ||
            !validLimit(desc.angularY) || !validLimit(desc.angularZ)) {
            return Fail(why, "constraint limit lower bound exceeds upper bound");
        }
        return true;
    }

    bool ValidateCharacterDesc(const CharacterDesc &desc, std::string *why)
    {
        if (desc.radius <= 0.f) {
            return Fail(why, "character radius must be positive");
        }
        if (desc.height < 0.f) {
            return Fail(why, "character height must be non-negative");
        }
        if (desc.stepHeight < 0.f) {
            return Fail(why, "character step height must be non-negative");
        }
        if (desc.slopeLimit < 0.f || desc.slopeLimit > 90.f) {
            return Fail(why, "character slope limit must be within [0, 90] degrees");
        }
        if (desc.up.Length() <= 0.f) {
            return Fail(why, "character up axis must be non-zero");
        }
        return true;
    }

    void InteractionLayers::Reset()
    {
        layers.clear();
    }

    bool InteractionLayers::Register(const std::string &name, uint32_t bit)
    {
        if (name.empty() || bit == 0) {
            return false;
        }
        for (const auto &layer : layers) {
            if (layer.first == name) {
                return layer.second == bit;
            }
            if (layer.second == bit) {
                return false;
            }
        }
        layers.emplace_back(name, bit);
        return true;
    }

    bool InteractionLayers::Find(const std::string &name, uint32_t &bit) const
    {
        for (const auto &layer : layers) {
            if (layer.first == name) {
                bit = layer.second;
                return true;
            }
        }
        return false;
    }

    bool InteractionLayers::IsRegistered(const std::string &name) const
    {
        uint32_t bit = 0;
        return Find(name, bit);
    }

    void PhysicsStepper::SetConfig(const PhysicsStepConfig &cfg)
    {
        config = cfg;
        if (config.fixedDelta <= 0.f) {
            config.fixedDelta = 1.f / 60.f;
        }
        if (config.maxSubSteps == 0) {
            config.maxSubSteps = 1;
        }
        accumulator = std::min(accumulator, config.fixedDelta * static_cast<float>(config.maxSubSteps));
    }

    uint32_t PhysicsStepper::Advance(float frameDelta)
    {
        if (frameDelta <= 0.f) {
            return 0;
        }
        accumulator += frameDelta;

        const float maxAccum = config.fixedDelta * static_cast<float>(config.maxSubSteps);
        if (accumulator > maxAccum) {
            // Drop excess time rather than spiral; the simulation cannot catch up.
            accumulator = maxAccum;
        }

        const uint32_t steps = static_cast<uint32_t>(accumulator / config.fixedDelta);
        accumulator -= static_cast<float>(steps) * config.fixedDelta;
        frame += steps;
        return steps;
    }

    float PhysicsStepper::GetInterpolationAlpha() const
    {
        return accumulator / config.fixedDelta;
    }

    void PhysicsStepper::Reset()
    {
        accumulator = 0.f;
        frame       = 0;
    }

} // namespace sky::phy
