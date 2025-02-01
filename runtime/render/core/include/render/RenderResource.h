//
// Created by blues on 2024/7/24.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <core/name/Name.h>
#include <core/util/Uuid.h>
#include <rhi/Stream.h>
#include <rhi/Queue.h>
#include <string>

namespace sky {

    class RenderResource : public RefObject {
    public:
        RenderResource() = default;
        explicit RenderResource(const Name &name_) : name(name_) {} // NOLINT
        ~RenderResource() override = default;

        void SetName(const Name &name_) { name = name_; }
        const Name &GetName() const { return name; }
    protected:
        Name name;
    };

    class IStreamableResource : public RenderResource {
    public:
        IStreamableResource() = default;
        explicit IStreamableResource(const Name &name_) : RenderResource(name_) {}
        ~IStreamableResource() override = default;

        void SetResourceID(const Uuid &id)
        {
            resID = id;
        }

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

        Uuid resID;
        rhi::Queue* uploadQueue = nullptr;
        rhi::TransferTaskHandle uploadHandle = 0;
    };
    using RDStreamableResourcePtr = CounterPtr<IStreamableResource>;

} // namespace sky