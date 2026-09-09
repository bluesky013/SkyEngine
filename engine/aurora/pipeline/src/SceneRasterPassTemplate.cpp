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

        auto &boundsPool = mScene->Pool<Bounds>();
        auto &itemPool   = mScene->Pool<RenderItem>();

        for (const auto &decl : mQueueDecls) {
            std::vector<std::pair<float, DrawItem>> gathered; // (view depth, item)

            for (uint32_t i = 0; i < boundsPool.Size(); ++i) {
                const Bounds &bounds = boundsPool.Data(i);
                if (mView != nullptr && !mView->FrustumCulling(bounds.worldBounds)) {
                    continue;
                }

                const EntityId entity = boundsPool.DenseEntity(i);
                const auto *ri = itemPool.Get(entity);
                if (ri == nullptr) {
                    continue;
                }
                if (decl.tag != Name{} && ri->techniqueTag != decl.tag) {
                    continue;
                }

                const Vector3 center = (bounds.worldBounds.min + bounds.worldBounds.max) * 0.5f;
                const float depth = mView != nullptr ? mView->ViewSpaceDepth(center) : 0.f;
                gathered.emplace_back(depth, ri->item);
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
