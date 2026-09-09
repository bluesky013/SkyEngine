//
// Scene raster pass template implementation.
//

#include <aurora/pipeline/SceneRasterPassTemplate.h>

#include <algorithm>

namespace sky::aurora {

    uint32_t SceneRasterPassTemplate::DeclareQueue(SceneRasterPassBuilder &builder,
                                                   const Name &queueName,
                                                   const Name &tag,
                                                   QueueSortPolicy sortPolicy)
    {
        const uint32_t index = builder.AddQueue(queueName, sortPolicy, tag);
        mQueueDecls.push_back(QueueDecl{queueName, tag, sortPolicy, index});
        return index;
    }

    void SceneRasterPassTemplate::Collect(SceneRasterPassBuilder &builder)
    {
        if (mScene == nullptr) {
            return;
        }

        const auto &primitives = mScene->GetPrimitives();

        for (const auto &decl : mQueueDecls) {
            // gather
            std::vector<std::pair<float, DrawItem>> gathered; // (view depth, item)
            gathered.reserve(primitives.size());

            for (const auto *prim : primitives) {
                if (prim == nullptr) {
                    continue;
                }
                if (mView != nullptr && !mView->FrustumCulling(prim->worldBounds)) {
                    continue;
                }

                GatherContext ctx{};
                ctx.tag  = decl.tag;
                ctx.view = mView;
                prim->GatherRenderItem(ctx);
                if (!ctx.gathered) {
                    continue;
                }

                const Vector3 center = (prim->worldBounds.min + prim->worldBounds.max) * 0.5f;
                const float depth = mView != nullptr ? mView->ViewSpaceDepth(center) : 0.f;
                gathered.emplace_back(depth, ctx.item);
            }

            // sort by policy
            if (decl.sort == QueueSortPolicy::FRONT_TO_BACK) {
                std::stable_sort(gathered.begin(), gathered.end(),
                                 [](const auto &a, const auto &b) { return a.first < b.first; });
            } else if (decl.sort == QueueSortPolicy::BACK_TO_FRONT) {
                std::stable_sort(gathered.begin(), gathered.end(),
                                 [](const auto &a, const auto &b) { return a.first > b.first; });
            }

            for (auto &[depth, item] : gathered) {
                (void)depth;
                builder.AddDrawItem(decl.index, item);
            }
        }
    }

} // namespace sky::aurora
