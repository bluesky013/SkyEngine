//
// Created by blues on 2024/9/1.
//

#include <bullet/debug/BulletDebugDraw.h>

namespace sky::phy {

    namespace {
        Vector3 FromBullet(const btVector3 &vec)
        {
            return Vector3{vec.x(), vec.y(), vec.z()};
        }

        constexpr int32_t DEFAULT_DEBUG_DRAW_FLAG =
            btIDebugDraw::DBG_DrawWireframe |
            btIDebugDraw::DBG_DrawAabb |
            btIDebugDraw::DBG_DrawContactPoints |
            btIDebugDraw::DBG_NoDeactivation |
            btIDebugDraw::DBG_DrawConstraints;
    } // namespace

    BulletDebugDraw::BulletDebugDraw()
        : debugMode(DEFAULT_DEBUG_DRAW_FLAG)
    {
    }

    void BulletDebugDraw::drawLine(const btVector3 &from, const btVector3 &to, const btVector3 &color)
    {
        const float rgba[4] = {color.x(), color.y(), color.z(), 1.f};
        geometry.AddLine(FromBullet(from), FromBullet(to), rgba);
    }

    void BulletDebugDraw::drawContactPoint(const btVector3 &pointOnB, const btVector3 &normalOnB,
                                           btScalar distance, int lifeTime, const btVector3 &color)
    {
        drawLine(pointOnB, pointOnB + normalOnB * distance, color);
        drawLine(pointOnB, pointOnB + normalOnB * 0.01f, btVector3{0.f, 0.f, 0.f});
    }

    void BulletDebugDraw::CollectGeometry(PhysicsDebugGeometry &out) const
    {
        out.lines.insert(out.lines.end(), geometry.lines.begin(), geometry.lines.end());
        out.triangles.insert(out.triangles.end(), geometry.triangles.begin(), geometry.triangles.end());
    }

} // namespace sky::phy
