//
// Created by Zach Lee on 2023/4/7.
//

#include <scene_render/Mesh.h>
#include <rhi/Device.h>
#include <IRHI.h>

namespace sky::rhi {
    void Mesh::SetLocalSet(const DescriptorSetPtr &set)
    {
        descriptorSet = set;
        Buffer::Descriptor bufferDesc = {};
        bufferDesc.size = sizeof(LocalData);
        bufferDesc.usage = BufferUsageFlagBit::UNIFORM;
        bufferDesc.memory = MemoryType::CPU_TO_GPU;

        localBuffer = Interface<IRHI>::Get()->GetApi()->GetDevice()->CreateBuffer(bufferDesc)->CreateView({0, sizeof(LocalData)});
        Update();

        descriptorSet->BindBuffer(0, localBuffer);
        descriptorSet->Update();
    }

    void Mesh::Update()
    {
        auto *ptr = localBuffer->Map();
        memcpy(ptr, &localData, sizeof(LocalData));
        localBuffer->UnMap();
    }
}