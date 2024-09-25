//
// Created by blues on 2024/7/24.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <rhi/Stream.h>
#include <rhi/Queue.h>
#include <string>

namespace sky {

    class RenderResource : public RefObject {
    public:
        RenderResource() = default;
        explicit RenderResource(const std::string &name_) : name(name_) {} // NOLINT
        ~RenderResource() override = default;

        void SetName(const std::string &name_) { name = name_; }
        const std::string &GetName() const { return name; }
    protected:
        std::string name;
    };

    class IStreamableResource : public RenderResource {
    public:
        IStreamableResource() = default;
        explicit IStreamableResource(const std::string &name_) : RenderResource(name_) {}
        ~IStreamableResource() override = default;

        uint64_t Upload(rhi::Queue *queue)
        {
            uploadQueue = queue;
            return UploadImpl();
        }

        void Wait() const
        {
            if (uploadQueue != nullptr) {
                uploadQueue->Wait(uploadHandle);
            }
        }

        bool IsReady() const
        {
            return uploadQueue != nullptr && uploadQueue->HasComplete(uploadHandle);
        }

    protected:
        virtual uint64_t UploadImpl() = 0;

        rhi::Queue* uploadQueue = nullptr;
        rhi::TransferTaskHandle uploadHandle = 0;
    };
    using RDStreamableResourcePtr = CounterPtr<IStreamableResource>;

} // namespace sky