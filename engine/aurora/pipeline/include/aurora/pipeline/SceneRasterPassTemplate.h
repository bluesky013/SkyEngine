//
// Scene raster pass template: declares queues, collects DrawItems from the scene.
//

#pragma once

#include <aurora/pipeline/PipelinePass.h>
#include <aurora/scene/RenderScene.h>
#include <aurora/rdg/RenderGraph.h>

#include <vector>

namespace sky::aurora {

    class SceneRasterPassTemplate : public PipelinePass {
    public:
        explicit SceneRasterPassTemplate(const Name &name) : PipelinePass(name) {}
        ~SceneRasterPassTemplate() override = default;

        void SetScene(RenderScene *scene) { mScene = scene; }
        void SetView(SceneView *view)     { mView = view; }

        RenderScene *GetScene() const { return mScene; }
        SceneView   *GetView() const  { return mView; }

    protected:
        // clear recorded queue declarations (call at the start of BuildRDG each frame)
        void ResetQueues() { mQueueDecls.clear(); }

        // declare a queue; records the declaration for Collect and forwards to the builder
        uint32_t DeclareQueue(SceneRasterPassBuilder &builder,
                              const Name &queueName,
                              const Name &tag,
                              QueueSortPolicy sortPolicy);

        // default collection: per declared queue, frustum-cull + tag-filter + sort
        virtual void Collect(SceneRasterPassBuilder &builder);

    private:
        struct QueueDecl {
            Name            name;
            Name            tag;
            QueueSortPolicy sort  = QueueSortPolicy::NONE;
            uint32_t        index = 0;
        };

        std::vector<QueueDecl> mQueueDecls;

        RenderScene *mScene = nullptr;
        SceneView   *mView  = nullptr;
    };

} // namespace sky::aurora
