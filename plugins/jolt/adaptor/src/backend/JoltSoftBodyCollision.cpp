#include "JoltConversion.h"
#include "JoltWorld.h"

#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/TransformedShape.h>

#include <algorithm>
#include <cmath>
#include <tuple>

namespace sky::phy {
    namespace {
        uint64_t ContactFeature(const JPH::CollideShapeResult &hit, JPH::QuatArg rotation, JPH::Vec3Arg normal)
        {
            const auto local = rotation.Conjugated() * normal;
            uint32_t   axis  = 0;
            if (std::abs(local.GetY()) > std::abs(local.GetX()))
                axis = 1;
            if (std::abs(local.GetZ()) > std::abs(local[axis]))
                axis = 2;
            return (uint64_t(hit.mSubShapeID2.GetValue()) << 32) | (axis * 2 + (local[axis] < 0 ? 1 : 0));
        }

        bool SameRotation(const Transform &a, const Transform &b)
        {
            const auto first  = ToJolt(a.rotation);
            const auto second = ToJolt(b.rotation);
            return first.IsClose(second, 1e-12f) || first.IsClose(-second, 1e-12f);
        }

        bool FiniteVectorMath(JPH::Vec3Arg value)
        {
            return Finite(FromJolt(value)) && Finite(value.LengthSq());
        }

        bool FiniteBounds(const JPH::AABox &bounds)
        {
            return Finite(FromJolt(bounds.mMin)) && Finite(FromJolt(bounds.mMax)) && Finite(FromJolt(bounds.mMax - bounds.mMin)) &&
                   Finite(FromJolt(bounds.mMax + bounds.mMin));
        }
    } // namespace

    PhysicsStatus JoltWorld::QuerySoftBodyContacts(const SoftBodyCollisionQuery &query, std::vector<SoftBodyCollisionContact> &out) const
    {
        if (!Ready())
            return status;
        if (!Finite(query.start) || !Finite(query.end) || !Finite(query.radius) || query.radius <= 0.f || !Finite(query.deltaTime) ||
            query.deltaTime <= 0.f) {
            Fail(PhysicsError::InvalidArgument, "Soft body collision query must contain finite positions, positive radius and duration");
            return status;
        }
        if (activation || collisionStepDelta <= 0.f || query.step != stats.stepCount ||
            std::abs(query.deltaTime - collisionStepDelta) > collisionStepDelta * 1e-5f) {
            Fail(PhysicsError::InvalidState, "Soft body collision query must address the most recently completed rigid step");
            return status;
        }

        const auto overflow = [&]() {
            Fail(PhysicsError::InvalidArgument, "Soft body collision query exceeds finite float geometry arithmetic");
            return status;
        };
        const auto start  = ToJolt(query.start);
        const auto end    = ToJolt(query.end);
        auto       bounds = JPH::AABox::sFromTwoPoints(start, end);
        bounds.ExpandBy(JPH::Vec3::sReplicate(query.radius));
        if (!Finite(query.radius * query.radius) || !FiniteVectorMath(end - start) || !FiniteBounds(bounds))
            return overflow();
        JPH::SphereShape                      sphere(query.radius);
        std::vector<SoftBodyCollisionContact> next;

        for (const auto &entry : slots) {
            if (!entry || entry->kind != Kind::Body || !entry->added || entry->desc.isTrigger || !FilterAccepts(query.filter, entry->filter))
                continue;
            const auto rotation     = ToJolt(entry->previous.rotation);
            const auto bodyStart    = ToJolt(entry->previous.translation);
            const auto displacement = ToJolt(entry->current.translation - entry->previous.translation);
            const auto relative     = end - start - displacement;
            const auto velocity     = displacement / collisionStepDelta;
            const auto center       = bodyStart + rotation * entry->shape->GetCenterOfMass();
            if (!FiniteVectorMath(displacement) || !FiniteVectorMath(relative) || !FiniteVectorMath(velocity) || !Finite(FromJolt(center)))
                return overflow();
            JPH::TransformedShape obstacle(center, rotation, entry->shape, entry->body);
            auto                  obstacleBounds = obstacle.GetWorldSpaceBounds();
            auto                  endpoint       = obstacle;
            endpoint.SetWorldTransform(ToJolt(entry->current.translation), ToJolt(entry->current.rotation), JPH::Vec3::sReplicate(1));
            obstacleBounds.Encapsulate(endpoint.GetWorldSpaceBounds());
            const bool rotating = !SameRotation(entry->previous, entry->current);
            if (rotating) {
                const auto  local = entry->shape->GetLocalBounds();
                const float reach = std::max(local.mMin.Length(), local.mMax.Length()) + entry->shape->GetCenterOfMass().Length();
                if (!Finite(reach))
                    return overflow();
                auto rotationBounds = JPH::AABox::sFromTwoPoints(bodyStart, bodyStart + displacement);
                rotationBounds.ExpandBy(JPH::Vec3::sReplicate(reach));
                obstacleBounds.Encapsulate(rotationBounds);
            }
            if (!FiniteBounds(obstacleBounds))
                return overflow();
            if (!bounds.Overlaps(obstacleBounds))
                continue;
            if (rotating) {
                Fail(PhysicsError::Unsupported, "Soft body collision queries do not support rotating obstacles");
                return status;
            }
            const auto append = [&](const JPH::CollideShapeResult &hit, float fraction, JPH::Vec3Arg translation) {
                const auto axis = hit.mPenetrationAxis;
                if (!FiniteVectorMath(axis) || axis.LengthSq() < 1e-12f || !Finite(fraction) || fraction < 0.f || fraction > 1.f)
                    return false;
                const auto               normal = -axis.Normalized();
                SoftBodyCollisionContact contact;
                contact.object          = entry->id;
                contact.point           = FromJolt(hit.mContactPointOn2 + translation);
                contact.normal          = FromJolt(normal);
                contact.surfaceVelocity = FromJolt(velocity);
                contact.fraction        = fraction;
                contact.feature         = ContactFeature(hit, rotation, normal);
                contact.dynamic         = entry->desc.kind == BodyKind::Dynamic;
                if (!Finite(contact.point) || !Finite(contact.surfaceVelocity))
                    return false;
                next.push_back(contact);
                return true;
            };
            const auto rejectHit = [&]() {
                Fail(PhysicsError::BackendFailure, "Jolt returned an invalid soft body collision contact");
                return status;
            };

            // Freeze the obstacle at its start pose and sweep relative motion, then advance its plane to the end pose.
            JPH::ShapeCastSettings castSettings;
            castSettings.mReturnDeepestPoint = true;
            castSettings.SetBackFaceMode(JPH::EBackFaceMode::CollideWithBackFaces);
            if (relative.LengthSq() > 1e-12f) {
                const JPH::RShapeCast cast(&sphere, JPH::Vec3::sReplicate(1), JPH::RMat44::sTranslation(start), relative);
                JPH::AllHitCollisionCollector<JPH::CastShapeCollector> hits;
                obstacle.CastShape(cast, castSettings, JPH::RVec3::sZero(), hits);
                for (const auto &hit : hits.mHits) {
                    if (!append(hit, hit.mFraction, displacement))
                        return rejectHit();
                }
            }

            JPH::CollideShapeSettings overlapSettings;
            overlapSettings.mBackFaceMode = JPH::EBackFaceMode::CollideWithBackFaces;
            for (uint32_t endpointIndex = 0; endpointIndex < 2; ++endpointIndex) {
                const bool                                                final = endpointIndex == 1;
                JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> hits;
                const auto                                               &target = final ? endpoint : obstacle;
                target.CollideShape(&sphere, JPH::Vec3::sReplicate(1), JPH::RMat44::sTranslation(final ? end : start), overlapSettings,
                                    JPH::RVec3::sZero(), hits);
                for (const auto &hit : hits.mHits) {
                    if (final && hit.mPenetrationAxis.LengthSq() > 1e-12f) {
                        const auto normal = -hit.mPenetrationAxis.Normalized();
                        // A terminal MTD may select the opposite exit face after deep entry into the same convex leaf.
                        const bool oppositeExit = std::any_of(next.begin(), next.end(), [&](const auto &prior) {
                            return prior.object == entry->id && (prior.feature >> 32) == hit.mSubShapeID2.GetValue() && prior.fraction < 1.f &&
                                   ToJolt(prior.normal).Dot(normal) < -1e-4f;
                        });
                        if (oppositeExit)
                            continue;
                    }
                    if (!append(hit, final ? 1.f : 0.f, final ? JPH::Vec3::sZero() : displacement))
                        return rejectHit();
                }
            }
        }

        std::sort(next.begin(), next.end(), [](const auto &a, const auto &b) {
            return std::tie(a.object, a.feature, a.fraction, a.point.x, a.point.y, a.point.z) <
                   std::tie(b.object, b.feature, b.fraction, b.point.x, b.point.y, b.point.z);
        });
        next.erase(std::unique(next.begin(), next.end(),
                               [](const auto &a, const auto &b) {
                                   return a.object == b.object && a.feature == b.feature && ToJolt(a.normal).Dot(ToJolt(b.normal)) > 1.f - 1e-6f &&
                                          std::abs(ToJolt(a.normal).Dot(ToJolt(a.point - b.point))) < 1e-5f;
                               }),
                   next.end());
        out = std::move(next);
        return {};
    }
} // namespace sky::phy
