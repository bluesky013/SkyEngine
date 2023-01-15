//
// Created by Zach Lee on 2023/1/12.
//

#pragma once

#include <memory>
#include <core/std/Container.h>
#include <render/rendergraph/LayoutDataBase.h>

namespace sky {

    class RenderPipeline {
    public:
        RenderPipeline() = default;
        ~RenderPipeline() = default;

        void Init();

        void Tick();

        LayoutDataBase *GetLayoutDataBase() const;

    private:
        PmrUnSyncPoolRes memoryResource;
        std::unique_ptr<LayoutDataBase> layoutDB;
    };

} // namespace sky
