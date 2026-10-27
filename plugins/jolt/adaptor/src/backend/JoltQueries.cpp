//
// Created on 2026/10/26.
//

#include "JoltConversion.h"
#include "JoltWorld.h"

#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>

#include <algorithm>

namespace sky::phy {
    class QueryBodyFilter final : public JPH::BodyFilter {
    public:
        QueryBodyFilter(const JoltWorld &world, const PhysicsQueryFilter &filter) : world(world), filter(filter)
        {
        }
        bool ShouldCollide(const JPH::BodyID &id) const override
        {
            auto *entry = world.FromNative(id);
            return entry && (entry->filter.group & filter.mask) && !filter.Ignores(entry->id);
        }

    private:
        const JoltWorld          &world;
        const PhysicsQueryFilter &filter;
    };
    static bool ValidCast(const Vector3 &origin, const Vector3 &direction, float distance)
    {
        return Finite(origin) && Finite(direction) && Finite(distance) && distance > 0 && ToJolt(direction).LengthSq() > 1e-12f;
    }
    static bool Castable(const ShapeDesc &shape)
    {
        if (shape.type == ShapeType::TriangleMesh || shape.type == ShapeType::HeightField) {
            return false;
        }
        return shape.type != ShapeType::Compound || std::all_of(shape.children.begin(), shape.children.end(), Castable);
    }
    bool JoltWorld::Raycast(
        const Vector3 &origin, const Vector3 &direction, float maxDistance, const PhysicsQueryFilter &filter, PhysicsQueryHit &out) const
    {
        out = {};
        if (!Ready()) {
            return false;
        }
        if (!ValidCast(origin, direction, maxDistance)) {
            return Fail(PhysicsError::InvalidArgument, "Invalid ray");
        }
        JPH::RRayCast                                        ray(ToJolt(origin), ToJolt(direction).Normalized() * maxDistance);
        JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
        QueryBodyFilter                                      bodyFilter(*this, filter);
        JPH::RayCastSettings                                 settings;
        settings.SetBackFaceMode(JPH::EBackFaceMode::CollideWithBackFaces);
        native->GetNarrowPhaseQuery().CastRay(ray, settings, collector, {}, {}, bodyFilter);
        std::sort(collector.mHits.begin(), collector.mHits.end(), [this](const auto &a, const auto &b) {
            return a.mFraction != b.mFraction ? a.mFraction < b.mFraction : FromNative(a.mBodyID)->id < FromNative(b.mBodyID)->id;
        });
        if (collector.mHits.empty()) {
            return false;
        }
        const auto       &hit = collector.mHits.front();
        JPH::BodyLockRead lock(native->GetBodyLockInterface(), hit.mBodyID);
        if (!lock.Succeeded()) {
            return Fail(PhysicsError::InvalidState, "Query body unavailable");
        }
        auto point   = ray.GetPointOnRay(hit.mFraction);
        out.object   = FromNative(hit.mBodyID)->id;
        out.position = FromJolt(point);
        out.normal   = FromJolt(lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, point));
        out.distance = hit.mFraction * maxDistance;
        return true;
    }
    bool JoltWorld::Sweep(const ShapeDesc          &desc,
                          const Transform          &from,
                          const Vector3            &direction,
                          float                     maxDistance,
                          const PhysicsQueryFilter &filter,
                          PhysicsQueryHit          &out) const
    {
        out = {};
        if (!Ready()) {
            return false;
        }
        if (!ValidTransform(from) || !ValidCast(from.translation, direction, maxDistance)) {
            return Fail(PhysicsError::InvalidArgument, "Invalid sweep");
        }
        auto shape = Cook(desc, from.scale);
        if (!shape) {
            return false;
        }
        if (!Castable(desc)) {
            return Fail(PhysicsError::Unsupported, "Sweeps require convex shapes or convex compounds");
        }
        auto                                                   cast = JPH::RShapeCast::sFromWorldTransform(shape, JPH::Vec3::sReplicate(1),
                                                                                                           JPH::RMat44::sRotationTranslation(ToJolt(from.rotation), ToJolt(from.translation)),
                                                                                                           ToJolt(direction).Normalized() * maxDistance);
        JPH::AllHitCollisionCollector<JPH::CastShapeCollector> collector;
        QueryBodyFilter                                        bodyFilter(*this, filter);
        JPH::ShapeCastSettings                                 settings;
        settings.mReturnDeepestPoint = true;
        native->GetNarrowPhaseQuery().CastShape(cast, settings, JPH::RVec3::sZero(), collector, {}, {}, bodyFilter);
        std::sort(collector.mHits.begin(), collector.mHits.end(), [this](const auto &a, const auto &b) {
            return a.mFraction != b.mFraction ? a.mFraction < b.mFraction : FromNative(a.mBodyID2)->id < FromNative(b.mBodyID2)->id;
        });
        if (collector.mHits.empty()) {
            return false;
        }
        const auto &hit = collector.mHits.front();
        out.object      = FromNative(hit.mBodyID2)->id;
        out.position    = FromJolt(hit.mContactPointOn2);
        out.normal      = FromJolt(-hit.mPenetrationAxis.NormalizedOr(JPH::Vec3::sZero()));
        out.distance    = hit.mFraction * maxDistance;
        return true;
    }
    void JoltWorld::Overlap(const ShapeDesc                  &desc,
                            const Transform                  &transform,
                            const PhysicsQueryFilter         &filter,
                            std::vector<PhysicsQueryOverlap> &out) const
    {
        out.clear();
        if (!Ready()) {
            return;
        }
        if (!ValidTransform(transform)) {
            Fail(PhysicsError::InvalidArgument, "Invalid overlap transform");
            return;
        }
        auto shape = Cook(desc, transform.scale);
        if (!shape) {
            return;
        }
        if (!Castable(desc)) {
            Fail(PhysicsError::Unsupported, "Overlap queries require convex shapes or convex compounds");
            return;
        }
        auto matrix =
            JPH::RMat44::sRotationTranslation(ToJolt(transform.rotation), ToJolt(transform.translation)).PreTranslated(shape->GetCenterOfMass());
        JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
        QueryBodyFilter                                           bodyFilter(*this, filter);
        native->GetNarrowPhaseQuery().CollideShape(shape, JPH::Vec3::sReplicate(1), matrix, {}, JPH::RVec3::sZero(), collector, {}, {}, bodyFilter);
        for (const auto &hit : collector.mHits) {
            out.push_back({FromNative(hit.mBodyID2)->id});
        }
        std::sort(out.begin(), out.end(), [](const auto &a, const auto &b) { return a.object < b.object; });
        out.erase(std::unique(out.begin(), out.end(), [](const auto &a, const auto &b) { return a.object == b.object; }), out.end());
    }
} // namespace sky::phy
