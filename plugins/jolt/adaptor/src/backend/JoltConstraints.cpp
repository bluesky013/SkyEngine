//
// Created on 2026/10/26.
//

#include "JoltConversion.h"
#include "JoltWorld.h"

#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>

#include <algorithm>

namespace sky::phy {
    static bool ValidLimit(const ConstraintLimit &limit)
    {
        return !limit.limited || (Finite(limit.lower) && Finite(limit.upper) && limit.lower <= limit.upper);
    }
    static bool ValidMotor(const ConstraintMotor &motor)
    {
        return !motor.enabled || (motor.mode <= PhysicsMotorMode::Position && Finite(motor.targetPosition) && Finite(motor.targetVelocity) &&
                                  Finite(motor.maxForce) && Finite(motor.maxTorque) && motor.maxForce >= 0 && motor.maxTorque >= 0);
    }
    static JPH::MotorSettings MotorSettings(const ConstraintMotor &motor)
    {
        JPH::MotorSettings settings;
        settings.mSpringSettings.mFrequency = 10.f;
        settings.mSpringSettings.mDamping   = 1.f;
        settings.SetForceLimit(motor.maxForce);
        settings.SetTorqueLimit(motor.maxTorque);
        return settings;
    }
    static JPH::EMotorState MotorState(const ConstraintMotor &motor)
    {
        return !motor.enabled                             ? JPH::EMotorState::Off
               : motor.mode == PhysicsMotorMode::Position ? JPH::EMotorState::Position
                                                          : JPH::EMotorState::Velocity;
    }

    PhysicsObjectId JoltWorld::CreateConstraint(const ConstraintDesc &desc)
    {
        if (!Ready()) {
            return {};
        }
        auto *a = Resolve(desc.bodyA);
        auto *b = Resolve(desc.bodyB);
        if (!a || a->kind != Kind::Body || (desc.bodyB != INVALID_PHYSICS_OBJECT_ID && (!b || b->kind != Kind::Body)) || a == b ||
            (a->desc.kind != BodyKind::Dynamic && (!b || b->desc.kind != BodyKind::Dynamic))) {
            Fail(PhysicsError::InvalidArgument, "Constraint requires owned bodies and at least one dynamic body");
            return {};
        }
        if (desc.type > ConstraintType::Generic6DOF || !Finite(desc.pivotA) || !Finite(desc.pivotB) || !Finite(desc.axisA) || !Finite(desc.axisB) ||
            ToJolt(desc.axisA).LengthSq() < 1e-8f || ToJolt(desc.axisB).LengthSq() < 1e-8f || !Finite(desc.distance) || desc.distance < 0 ||
            !ValidLimit(desc.linearX) || !ValidLimit(desc.linearY) || !ValidLimit(desc.linearZ) || !ValidLimit(desc.angularX) ||
            !ValidLimit(desc.angularY) || !ValidLimit(desc.angularZ) || !ValidMotor(desc.linearMotor) || !ValidMotor(desc.angularMotor)) {
            Fail(PhysicsError::InvalidArgument, "Invalid constraint settings");
            return {};
        }
        const auto angularBound = [](const ConstraintLimit &l) { return !l.limited || (l.lower >= -JPH::JPH_PI && l.upper <= JPH::JPH_PI); };
        if (!angularBound(desc.angularX) || !angularBound(desc.angularY) || !angularBound(desc.angularZ)) {
            Fail(PhysicsError::InvalidArgument, "Angular limits must be within [-pi, pi]");
            return {};
        }
        if ((desc.type == ConstraintType::Fixed || desc.type == ConstraintType::Distance) &&
            (desc.linearMotor.enabled || desc.angularMotor.enabled)) {
            Fail(PhysicsError::Unsupported, "Fixed and distance constraints do not support motors");
            return {};
        }
        if ((desc.type == ConstraintType::Hinge && desc.linearMotor.enabled) || (desc.type == ConstraintType::Slider && desc.angularMotor.enabled)) {
            Fail(PhysicsError::Unsupported, "Motor axis does not match the constraint");
            return {};
        }
        const bool otherLinear  = desc.linearY.limited || desc.linearZ.limited;
        const bool otherAngular = desc.angularY.limited || desc.angularZ.limited;
        if (desc.type != ConstraintType::Generic6DOF &&
            (otherLinear || otherAngular || ((desc.type == ConstraintType::Fixed || desc.type == ConstraintType::Hinge) && desc.linearX.limited) ||
             ((desc.type == ConstraintType::Fixed || desc.type == ConstraintType::Slider || desc.type == ConstraintType::Distance) &&
              desc.angularX.limited))) {
            Fail(PhysicsError::Unsupported, "Limit axis is not supported by this constraint type");
            return {};
        }
        JPH::BodyID                      ids[] = {a->body, b ? b->body : JPH::BodyID()};
        JPH::Ref<JPH::TwoBodyConstraint> result;
        {
            JPH::BodyLockMultiWrite lock(native->GetBodyLockInterface(), ids, 2);
            auto                   *bodyA = lock.GetBody(0);
            auto                   *bodyB = b ? lock.GetBody(1) : &JPH::Body::sFixedToWorld;
            if (!bodyA || !bodyB) {
                Fail(PhysicsError::InvalidState, "Constraint body unavailable");
                return {};
            }
            // Engine pivots use object origin; Jolt's local constraint frame uses center of mass.
            auto pointA = ToJolt(desc.pivotA * a->current.scale) - a->shape->GetCenterOfMass();
            auto pointB = b ? ToJolt(desc.pivotB * b->current.scale) - b->shape->GetCenterOfMass() : ToJolt(desc.pivotB);
            auto axisA  = ToJolt(desc.axisA).Normalized();
            auto axisB  = ToJolt(desc.axisB).Normalized();
            switch (desc.type) {
            case ConstraintType::Fixed: {
                JPH::FixedConstraintSettings s;
                s.mSpace  = JPH::EConstraintSpace::LocalToBodyCOM;
                s.mPoint1 = pointA;
                s.mPoint2 = pointB;
                s.mAxisX1 = axisA;
                s.mAxisY1 = axisA.GetNormalizedPerpendicular();
                s.mAxisX2 = axisB;
                s.mAxisY2 = axisB.GetNormalizedPerpendicular();
                result    = s.Create(*bodyA, *bodyB);
                break;
            }
            case ConstraintType::Hinge: {
                JPH::HingeConstraintSettings s;
                s.mSpace       = JPH::EConstraintSpace::LocalToBodyCOM;
                s.mPoint1      = pointA;
                s.mPoint2      = pointB;
                s.mHingeAxis1  = axisA;
                s.mNormalAxis1 = axisA.GetNormalizedPerpendicular();
                s.mHingeAxis2  = axisB;
                s.mNormalAxis2 = axisB.GetNormalizedPerpendicular();
                if (desc.angularX.limited) {
                    if (desc.angularX.lower > 0 || desc.angularX.upper < 0) {
                        Fail(PhysicsError::Unsupported, "Hinge limits must include zero");
                        return {};
                    }
                    s.mLimitsMin = desc.angularX.lower;
                    s.mLimitsMax = desc.angularX.upper;
                }
                s.mMotorSettings = MotorSettings(desc.angularMotor);
                auto *hinge      = new JPH::HingeConstraint(*bodyA, *bodyB, s);
                hinge->SetMotorState(MotorState(desc.angularMotor));
                hinge->SetTargetAngularVelocity(desc.angularMotor.targetVelocity);
                hinge->SetTargetAngle(desc.angularMotor.targetPosition);
                result = hinge;
                break;
            }
            case ConstraintType::Slider: {
                JPH::SliderConstraintSettings s;
                s.mSpace       = JPH::EConstraintSpace::LocalToBodyCOM;
                s.mPoint1      = pointA;
                s.mPoint2      = pointB;
                s.mSliderAxis1 = axisA;
                s.mNormalAxis1 = axisA.GetNormalizedPerpendicular();
                s.mSliderAxis2 = axisB;
                s.mNormalAxis2 = axisB.GetNormalizedPerpendicular();
                if (desc.linearX.limited) {
                    if (desc.linearX.lower > 0 || desc.linearX.upper < 0) {
                        Fail(PhysicsError::Unsupported, "Slider limits must include zero");
                        return {};
                    }
                    s.mLimitsMin = desc.linearX.lower;
                    s.mLimitsMax = desc.linearX.upper;
                }
                s.mMotorSettings = MotorSettings(desc.linearMotor);
                auto *slider     = new JPH::SliderConstraint(*bodyA, *bodyB, s);
                slider->SetMotorState(MotorState(desc.linearMotor));
                slider->SetTargetVelocity(desc.linearMotor.targetVelocity);
                slider->SetTargetPosition(desc.linearMotor.targetPosition);
                result = slider;
                break;
            }
            case ConstraintType::Distance: {
                JPH::DistanceConstraintSettings s;
                s.mSpace       = JPH::EConstraintSpace::LocalToBodyCOM;
                s.mPoint1      = pointA;
                s.mPoint2      = pointB;
                s.mMinDistance = desc.linearX.limited ? desc.linearX.lower : desc.distance;
                s.mMaxDistance = desc.linearX.limited ? desc.linearX.upper : desc.distance;
                if (s.mMinDistance < 0) {
                    Fail(PhysicsError::InvalidArgument, "Distance limits must be nonnegative");
                    return {};
                }
                result = s.Create(*bodyA, *bodyB);
                break;
            }
            case ConstraintType::Generic6DOF: {
                JPH::SixDOFConstraintSettings s;
                s.mSpace                       = JPH::EConstraintSpace::LocalToBodyCOM;
                s.mPosition1                   = pointA;
                s.mPosition2                   = pointB;
                s.mAxisX1                      = axisA;
                s.mAxisY1                      = axisA.GetNormalizedPerpendicular();
                s.mAxisX2                      = axisB;
                s.mAxisY2                      = axisB.GetNormalizedPerpendicular();
                const ConstraintLimit limits[] = {desc.linearX, desc.linearY, desc.linearZ, desc.angularX, desc.angularY, desc.angularZ};
                for (int axis = 0; axis < 6; ++axis) {
                    if (limits[axis].limited) {
                        s.SetLimitedAxis(static_cast<JPH::SixDOFConstraintSettings::EAxis>(axis), limits[axis].lower, limits[axis].upper);
                    }
                }
                if ((desc.linearMotor.enabled && desc.linearX.limited && desc.linearX.lower == desc.linearX.upper) ||
                    (desc.angularMotor.enabled && desc.angularX.limited && desc.angularX.lower == desc.angularX.upper)) {
                    Fail(PhysicsError::InvalidArgument, "Cannot drive a locked 6DOF axis");
                    return {};
                }
                s.mMotorSettings[0] = MotorSettings(desc.linearMotor);
                s.mMotorSettings[3] = MotorSettings(desc.angularMotor);
                auto *six           = new JPH::SixDOFConstraint(*bodyA, *bodyB, s);
                six->SetMotorState(JPH::SixDOFConstraint::EAxis::TranslationX, MotorState(desc.linearMotor));
                six->SetMotorState(JPH::SixDOFConstraint::EAxis::RotationX, MotorState(desc.angularMotor));
                six->SetTargetVelocityCS(JPH::Vec3(desc.linearMotor.targetVelocity, 0, 0));
                six->SetTargetPositionCS(JPH::Vec3(desc.linearMotor.targetPosition, 0, 0));
                six->SetTargetAngularVelocityCS(JPH::Vec3(desc.angularMotor.targetVelocity, 0, 0));
                six->SetTargetOrientationCS(JPH::Quat::sRotation(JPH::Vec3::sAxisX(), desc.angularMotor.targetPosition));
                result = six;
                break;
            }
            }
        }
        if (!result) {
            Fail(PhysicsError::BackendFailure, "Constraint creation failed");
            return {};
        }
        auto &entry          = Allocate(Kind::Constraint);
        entry.constraint     = result;
        entry.constraintDesc = desc;
        native->AddConstraint(result);
        ++stats.constraintCount;
        if (!desc.collideConnected && b) {
            ++exclusions[std::minmax(a->id, b->id)];
            RefreshBody(*b);
        }
        RefreshBody(*a);
        return entry.id;
    }

    bool JoltWorld::DestroyConstraint(PhysicsObjectId id)
    {
        if (!Ready()) {
            return false;
        }
        auto *entry = Resolve(id);
        if (!entry || entry->kind != Kind::Constraint) {
            return Fail(PhysicsError::InvalidArgument, "Invalid constraint handle");
        }
        auto *a = Resolve(entry->constraintDesc.bodyA);
        auto *b = Resolve(entry->constraintDesc.bodyB);
        if (!entry->constraintDesc.collideConnected && a && b) {
            auto it = exclusions.find(std::minmax(a->id, b->id));
            if (it != exclusions.end() && --it->second == 0) {
                exclusions.erase(it);
            }
            RefreshBody(*a);
            RefreshBody(*b);
        }
        native->RemoveConstraint(entry->constraint);
        Release(id);
        --stats.constraintCount;
        return true;
    }
} // namespace sky::phy
