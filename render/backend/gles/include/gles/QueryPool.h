//
// Created by Zach Lee on 2023/7/15.
//

#pragma once

#include <rhi/QueryPool.h>
#include <gles/DevObject.h>

namespace sky::gles {

    class QueryPool : public rhi::QueryPool, public DevObject {
    public:
        explicit QueryPool(Device &dev) : DevObject(dev)
        {
        }
        ~QueryPool() override = default;

    private:
        friend class Device;
        bool Init(const Descriptor &desc);
    };
    using QueryPoolPtr = std::shared_ptr<QueryPool>;

} // namespace sky::gles