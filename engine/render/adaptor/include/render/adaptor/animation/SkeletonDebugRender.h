//
// Created by Zach Lee on 2025/7/9.
//

#pragma once

#include <animation/core/AnimationPose.h>
#include <animation/graph/AnimationNode.h>
#include <render/resource/Technique.h>
#include <render/debug/DebugRenderer.h>
#include <render/RenderPrimitive.h>

namespace sky {

    struct Bone;

    class SkeletonDebugRender {
    public:
        SkeletonDebugRender();
        ~SkeletonDebugRender() = default;

        void SetTechnique(const RDTechniquePtr& tech);
        void DrawPose(const AnimPose &pose_, const Transform &world);

        RenderPrimitive* GetPrimitive() { return primitive.get(); }
    private:
        void DrawBone(const Bone* bone, const Transform &world, bool drawLink);

        void DrawJoint(const Transform& bone);
        void DrawJointLink(const Transform& src, const Transform& dst);

        AnimFinalPose pose;

        RDTechniquePtr technique;
        std::unique_ptr<RenderPrimitive> primitive;
        std::unique_ptr<DebugRenderer> debugRenderer;
    };

} // namespace sky
