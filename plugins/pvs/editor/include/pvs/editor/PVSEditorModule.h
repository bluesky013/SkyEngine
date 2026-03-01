//
// Created by Zach Lee on 2026/2/23.
//

#pragma once

#include <pvs/PVSModule.h>
#include <pvs/editor/PVSWorldBuilder.h>
#include <pvs/editor/PVSVolume.h>

namespace sky::editor {

    class PVSEditorModule : public PVSModule, public IWorldBuilderGather {
    public:
        PVSEditorModule();
        ~PVSEditorModule() override = default;

        bool Init(const StartArguments &args) override;
        void Shutdown() override;

    private:
        void OnCreateRenderScene(RenderScene* scene) override;
        void Gather(std::list<CounterPtr<IWorldBuilder>> &builders) const override;

        EventBinder<IWorldBuilderGather> binder;
        std::unique_ptr<PVSVolumeCreator> volumeCreator;
    };

} // namespace sky::editor