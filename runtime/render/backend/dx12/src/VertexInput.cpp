//
// Created by blues on 2024/2/12.
//

#include <dx12/VertexInput.h>
#include <set>
#include <string>
#include <mutex>
#include <dx12/Conversion.h>
#include <core/platform/Platform.h>

namespace sky::dx {
    std::mutex SEMATIC_MUTEX;
    std::set<std::string> SEMATIC_NAME_STORAGE;

    bool VertexInput::Init(const Descriptor &desc)
    {
        storage.reserve(desc.attributes.size());
        for (const auto &attr : desc.attributes) {
            storage.emplace_back();
            auto &back = storage.back();
            {
                std::lock_guard<std::mutex> lock(SEMATIC_MUTEX);
                auto iter = SEMATIC_NAME_STORAGE.find(attr.sematic);
                if (iter == SEMATIC_NAME_STORAGE.end()) {
                    iter = SEMATIC_NAME_STORAGE.emplace(attr.sematic).first;
                }
                back.SemanticName = iter->c_str();
                back.SemanticIndex = 0;

                back.AlignedByteOffset = attr.offset;
                back.InputSlot = attr.binding;
                back.Format = FromRHI(attr.format);

                SKY_ASSERT(attr.binding < desc.bindings.size());
                const auto &binding = desc.bindings[attr.binding];
                back.InputSlotClass = binding.inputRate == rhi::VertexInputRate::PER_INSTANCE ?
                    D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
                back.InstanceDataStepRate = 1;
            }
        }

        vertexDesc.NumElements = static_cast<UINT>(storage.size());
        vertexDesc.pInputElementDescs = storage.data();
        return true;
    }

} // namespace sky::dx
