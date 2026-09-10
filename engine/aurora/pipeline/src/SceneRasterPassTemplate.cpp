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

        // v1: cull-only pass over the Bounds pool; draw items are produced once
        // the technique design lands (no RenderItem component for now)
        auto &boundsPool = mScene->Pool<Bounds>();

        for (const auto &decl : mQueueDecls) {
            (void)decl;
            uint32_t visibleCount = 0;
            for (uint32_t i = 0; i < boundsPool.Size(); ++i) {
                if (mView != nullptr && !mView->FrustumCulling(boundsPool.Data(i).worldBounds)) {
                    continue;
                }
                ++visibleCount;
            }
            (void)visibleCount;
            (void)builder;
        }
    }

} // namespace sky::aurora
