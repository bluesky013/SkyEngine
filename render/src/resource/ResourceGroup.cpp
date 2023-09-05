//
// Created by Zach Lee on 2023/8/28.
//

#include <render/resource/ResourceGroup.h>
#include <render/Renderer.h>

namespace sky {

    ResourceGroup::~ResourceGroup()
    {
        Renderer::Get()->GetResourceGC()->CollectDescriptorSet(set);
    }

} // namespace sky
