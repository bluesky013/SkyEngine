#pragma once

#include "MetalDescriptorEncoder.h"
#include <aurora/rhi/DescriptorBatch.h>

#include <map>

namespace sky::aurora {
    class MetalDescriptorBatch : public DescriptorBatch {
    public:
        explicit MetalDescriptorBatch(const Device &device) : device(&device)
        {
        }
        ~MetalDescriptorBatch() override = default;
        bool WriteBuffer(ResourceGroup *group, uint32_t binding, Buffer *buffer, uint64_t offset, uint64_t range, uint32_t arrayElement = 0) override
        {
            auto *encoder = Find(group);
            if (encoder == nullptr || !encoder->WriteBuffer(binding, buffer, offset, range, arrayElement)) {
                failed = true;
                return false;
            }
            return true;
        }
        bool WriteImage(ResourceGroup *group, uint32_t binding, Image *image, ImageLayout layout, uint32_t arrayElement = 0) override
        {
            auto *encoder = Find(group);
            if (encoder == nullptr || !encoder->WriteImage(binding, image, layout, arrayElement)) {
                failed = true;
                return false;
            }
            return true;
        }
        bool WriteSampler(ResourceGroup *group, uint32_t binding, Sampler *sampler, uint32_t arrayElement = 0) override
        {
            auto *encoder = Find(group);
            if (encoder == nullptr || !encoder->WriteSampler(binding, sampler, arrayElement)) {
                failed = true;
                return false;
            }
            return true;
        }
        bool Flush() override
        {
            if (failed) {
                return false;
            }
            for (auto &[group, encoder] : encoders) {
                if (!encoder->Ready()) {
                    failed = true;
                    return false;
                }
            }
            for (auto &[group, encoder] : encoders) {
                encoder->Commit();
            }
            encoders.clear();
            return true;
        }
        void Reset() override
        {
            encoders.clear();
            failed = false;
        }

    private:
        MetalDescriptorEncoder *Find(ResourceGroup *group)
        {
            if (failed) {
                return nullptr;
            }
            if (group == nullptr || group->GetDevice() != device) {
                failed = true;
                return nullptr;
            }
            if (group->IsParameterSnapshot()) {
                failed = true;
                return nullptr;
            }
            auto *native  = static_cast<MetalResourceGroup *>(group);
            auto &encoder = encoders[native];
            if (!encoder) {
                encoder = std::make_unique<MetalDescriptorEncoder>(*native);
            }
            return encoder.get();
        }
        const Device                                                           *device = nullptr;
        std::map<MetalResourceGroup *, std::unique_ptr<MetalDescriptorEncoder>> encoders;
        bool                                                                    failed = false;
    };
} // namespace sky::aurora
