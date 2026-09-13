//
// Created on 2026/09/13.
//

#include <D3D12ResourceGroupLayout.h>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraDX12";

namespace sky::aurora {

    D3D12ResourceGroupLayout::D3D12ResourceGroupLayout(D3D12Device &dev)
        : device(dev)
    {
    }

    bool D3D12ResourceGroupLayout::Init(const Descriptor &desc)
    {
        bindings = desc.bindings;

        cbvSrvUavOffsets.resize(bindings.size());
        samplerOffsets.resize(bindings.size());

        for (size_t i = 0; i < bindings.size(); ++i) {
            const auto &b = bindings[i];

            if (b.flags & DescriptorBindingFlagBit::VARIABLE_COUNT) {
                LOG_E(TAG, "VARIABLE_COUNT bindings not supported in this change");
                return false;
            }
            if (b.count == 0) {
                LOG_E(TAG, "binding count must be >= 1 (binding %u)", b.binding);
                return false;
            }
            for (size_t j = 0; j < i; ++j) {
                if (bindings[j].binding == b.binding) {
                    LOG_E(TAG, "duplicate binding %u in layout", b.binding);
                    return false;
                }
            }

            cbvSrvUavOffsets[i] = cbvSrvUavCount;
            samplerOffsets[i]   = samplerCount;

            if (b.type == DescriptorType::SAMPLER) {
                samplerCount += b.count;
            } else if (b.type == DescriptorType::COMBINED_IMAGE_SAMPLER) {
                cbvSrvUavCount += b.count;
                samplerCount += b.count;
            } else {
                cbvSrvUavCount += b.count;
            }
        }

        return true;
    }

    uint32_t D3D12ResourceGroupLayout::FindBinding(uint32_t binding) const
    {
        for (size_t i = 0; i < bindings.size(); ++i) {
            if (bindings[i].binding == binding) {
                return static_cast<uint32_t>(i);
            }
        }
        return INVALID_INDEX;
    }

} // namespace sky::aurora
