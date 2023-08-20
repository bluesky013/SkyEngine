//
// Created by Zach Lee on 2023/4/16.
//

#include <render/rdg/RenderGraphContext.h>
#include <render/RHI.h>

namespace sky::rdg {

    const rhi::SemaphorePtr &SemaphorePool::Acquire()
    {
        if (index >= imageAvailableSemaList.size()) {
            imageAvailableSemaList.emplace_back(RHI::Get()->GetDevice()->CreateSema({}));
        }
        return imageAvailableSemaList[index];
    }

    void SemaphorePool::Reset()
    {
        index = 0;
    }

} // namespace sky::rdg