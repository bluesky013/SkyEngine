//
// Created by Zach Lee on 2022/5/24.
//


#include <render/resources/DescirptorGroup.h>
#include <render/resources/GlobalResource.h>

namespace sky {

    bool DescriptorGroup::IsValid() const
    {
        return !!set;
    }

    void DescriptorGroup::UpdateTexture(uint32_t binding, RDTexturePtr texture)
    {

    }

    void DescriptorGroup::UpdateBuffer(uint32_t binding, const BufferView& buffer)
    {

    }

    void DescriptorGroup::Update()
    {
        
    }
}