//
// Created by Zach Lee on 2023/5/28.
//

#include <mtl/Queue.h>

namespace sky::mtl {

    Queue::~Queue()
    {
        if (queue) {
            [queue release];
        }
    }

    rhi::TransferTaskHandle Queue::UploadImage(const rhi::ImagePtr &image, const std::vector<rhi::ImageUploadRequest> &requests)
    {
        return CreateTask([]() {

            });
    }

    rhi::TransferTaskHandle Queue::UploadBuffer(const rhi::BufferPtr &image, const std::vector<rhi::BufferUploadRequest> &requests)
    {
        return CreateTask([]() {

        });
    }

} // namespace sky::mtl

