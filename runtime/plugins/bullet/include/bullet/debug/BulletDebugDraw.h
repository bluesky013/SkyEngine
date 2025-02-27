//
// Created by blues on 2024/9/1.
//

#pragma once

#include <physics/PhysicsDebugDraw.h>
#include <LinearMath/btIDebugDraw.h>
#include <render/debug/DebugRenderer.h>
#include <render/RenderPrimitive.h>
#include <cstdint>
#include <memory>

namespace sky::phy {

    class BulletDebugDraw : public PhysicsDebugDraw, public btIDebugDraw {
    public:
        BulletDebugDraw();
        ~BulletDebugDraw() override = default;

        void SetTechnique(const RDGfxTechPtr &tech) override;
        RenderPrimitive* GetPrimitive() const { return primitive.get(); }
    private:
        void drawLine(const btVector3& from, const btVector3& to, const btVector3& color) override;
        void drawContactPoint(const btVector3& pointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color) override;
        void reportErrorWarning(const char* warningString) override;
        void draw3dText(const btVector3& location, const char* textString) override;
        void setDebugMode(int debugMode_) override { debugMode = debugMode_; }
        int getDebugMode() const override { return debugMode; }

        void clearLines() override;
        void flushLines() override;

        int32_t debugMode = 0;

        std::unique_ptr<RenderPrimitive> primitive;
        std::unique_ptr<DebugRenderer> debugRenderer;
    };

} // namespace sky::phy