//
// Created by Zach Lee on 2025/7/9.
//

#include <render/adaptor/animation/SkeletonDebugRender.h>
#include <animation/core/Skeleton.h>

namespace sky {

    SkeletonDebugRender::SkeletonDebugRender()
        : primitive(std::make_unique<RenderPrimitive>())
    {
    }

    void SkeletonDebugRender::DrawPose(const AnimPose &pose_, const Transform &world)
    {
        if (pose_.skeleton == nullptr) {
            return;
        }

        pose = AnimFinalPose(pose_);

        if (!debugRenderer) {
            debugRenderer = std::make_unique<DebugRenderer>();
        }
        debugRenderer->Reset();

        const auto& roots = pose.skeleton->GetRoots();
        for (const auto& root : roots) {
            DrawBone(root, world, false);
        }

        debugRenderer->Render(primitive.get());
    }

    void SkeletonDebugRender::DrawBone(const Bone* bone, const Transform &world, bool drawLink) // NOLINT
    {
        SKY_ASSERT(bone != nullptr);

        const auto &trans = pose.transforms[bone->index];
        Transform current = world * trans;

        DrawJoint(current);
        if (drawLink) {
            DrawJointLink(world, current);
        }

        for (const auto& childIdx : bone->children) {
            const auto* child = pose.skeleton->GetBoneByIndex(childIdx);
            DrawBone(child, current, true);
        }
    }

    void SkeletonDebugRender::SetTechnique(const sky::RDTechniquePtr &tech)
    {
        // RenderBatch batch = {tech};
        // batch.topo = rhi::PrimitiveTopology::LINE_LIST;
        // primitive->sections.clear();
        // primitive->sections.emplace_back();
        // primitive->sections[0].batches.emplace_back(batch);
    }

    void SkeletonDebugRender::DrawJoint(const Transform& bone)
    {
        debugRenderer->DrawSphere(Sphere(bone.translation, 0.5f));
    }

    void SkeletonDebugRender::DrawJointLink(const Transform& src, const Transform& dst)
    {
        debugRenderer->DrawLine(src.translation, dst.translation);
    }

} // namespace sky
