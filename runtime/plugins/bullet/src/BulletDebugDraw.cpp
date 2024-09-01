//
// Created by blues on 2024/9/1.
//

#include <bullet/BulletDebugDraw.h>

namespace sky::phy {
    static constexpr int32_t DEFAULT_DEBUG_DRAW_FLAG =
            btIDebugDraw::DBG_DrawWireframe |
            btIDebugDraw::DBG_DrawAabb |
            btIDebugDraw::DBG_DrawContactPoints |
            btIDebugDraw::DBG_NoDeactivation |
            btIDebugDraw::DBG_DrawConstraints;

    BulletDebugDraw::BulletDebugDraw()
        : debugMode(DEFAULT_DEBUG_DRAW_FLAG)
    {
    }

    void BulletDebugDraw::drawLine(const btVector3& from, const btVector3& to, const btVector3& color)
    {

    }

    void BulletDebugDraw::drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color)
    {

    }

    void BulletDebugDraw::reportErrorWarning(const char* warningString)
    {
    }

    void BulletDebugDraw::draw3dText(const btVector3& location, const char* textString)
    {
    }

    void BulletDebugDraw::clearLines()
    {

    }

    void BulletDebugDraw::flushLines()
    {

    }
} // namespace sky::phy