//
// Created by blues on 2024/9/1.
//

#pragma once

#include <physics/PhysicsDebugDraw.h>
#include <LinearMath/btIDebugDraw.h>

#include <cstdint>

namespace sky::phy {

    // Bullet debug drawer producing render-agnostic geometry (no render adapter / primitive).
    class BulletDebugDraw : public PhysicsDebugDraw, public btIDebugDraw {
    public:
        BulletDebugDraw();
        ~BulletDebugDraw() override = default;

        void CollectGeometry(PhysicsDebugGeometry &out) const override;

    private:
        void drawLine(const btVector3 &from, const btVector3 &to, const btVector3 &color) override;
        void drawContactPoint(const btVector3 &pointOnB, const btVector3 &normalOnB, btScalar distance,
                              int lifeTime, const btVector3 &color) override;
        void reportErrorWarning(const char *warningString) override {}
        void draw3dText(const btVector3 &location, const char *textString) override {}
        void setDebugMode(int debugMode_) override { debugMode = debugMode_; }
        int  getDebugMode() const override { return debugMode; }
        void clearLines() override { geometry.Clear(); }
        void flushLines() override {}

        int32_t             debugMode = 0;
        PhysicsDebugGeometry geometry;
    };

} // namespace sky::phy
