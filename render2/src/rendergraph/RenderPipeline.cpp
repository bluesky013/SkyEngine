//
// Created by Zach Lee on 2023/1/12.
//

#include <render/rendergraph/RenderPipeline.h>
namespace sky {

    void RenderPipeline::Init()
    {
        layoutDB = std::make_unique<LayoutDataBase>();
        layoutDB->memoryResource = &memoryResource;
    }

    LayoutDataBase *RenderPipeline::GetLayoutDataBase() const
    {
        return layoutDB.get();
    }

} // namespace sky
