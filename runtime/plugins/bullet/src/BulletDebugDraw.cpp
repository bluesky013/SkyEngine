//
// Created by blues on 2024/9/1.
//

#include <bullet/BulletDebugDraw.h>

namespace sky::phy {

    static Vector3 FromBullet(const btVector3 &vec)
    {
        return Vector3{vec.x(), vec.y(), vec.z()};
    }

    static constexpr int32_t DEFAULT_DEBUG_DRAW_FLAG =
            btIDebugDraw::DBG_DrawWireframe |
            btIDebugDraw::DBG_DrawAabb |
            btIDebugDraw::DBG_DrawContactPoints |
            btIDebugDraw::DBG_NoDeactivation |
            btIDebugDraw::DBG_DrawConstraints;

    BulletDebugDraw::BulletDebugDraw()
        : debugMode(DEFAULT_DEBUG_DRAW_FLAG)
        , debugRenderer(std::make_unique<DebugRenderer>())
    {
    }

    void BulletDebugDraw::drawLine(const btVector3& from, const btVector3& to, const btVector3& color)
    {
        debugRenderer->SetColor(Color{color.x(), color.y(), color.z(), 1.f});
        debugRenderer->DrawLine(FromBullet(from), FromBullet(to));
    }

    void BulletDebugDraw::drawContactPoint(const btVector3& pointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color)
    {
        drawLine(pointOnB, pointOnB + normalOnB * distance, color);
        drawLine(pointOnB, pointOnB + normalOnB * 0.01f, btVector3{0, 0, 0});
    }

    void BulletDebugDraw::reportErrorWarning(const char* warningString)
    {
    }

    void BulletDebugDraw::draw3dText(const btVector3& location, const char* textString)
    {
    }

    void BulletDebugDraw::clearLines()
    {
        debugRenderer->Reset();
    }

    void BulletDebugDraw::flushLines()
    {
    }
} // namespace sky::phy