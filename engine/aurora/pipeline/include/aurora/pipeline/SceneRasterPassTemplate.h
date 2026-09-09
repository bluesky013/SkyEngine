//
// Scene raster pass template: declares queues, leaves collection to subclasses.
//

#pragma once

#include <aurora/pipeline/PipelinePass.h>
#include <aurora/rdg/RenderGraph.h>

namespace sky::aurora {

    class SceneRasterPassTemplate : public PipelinePass {
    public:
        explicit SceneRasterPassTemplate(const Name &name) : PipelinePass(name) {}
        ~SceneRasterPassTemplate() override = default;

    protected:
        // declare a queue on the pass builder; returns queue index for AddDrawItem
        static uint32_t DeclareQueue(SceneRasterPassBuilder &builder,
                                     const Name &queueName,
                                     QueueSortPolicy sortPolicy)
        {
            return builder.AddQueue(queueName, sortPolicy);
        }

        // collection hook: fill DrawItems into queues (scene integration comes later)
        virtual void Collect(SceneRasterPassBuilder &builder) { (void)builder; }
    };

} // namespace sky::aurora
