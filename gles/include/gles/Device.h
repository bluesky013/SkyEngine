//
// Created by Zach on 2023/1/30.
//

#pragma once

#include <rhi/Device.h>
#include <gles/Context.h>
#include <gles/PBuffer.h>
#include <gles/Swapchain.h>
#include <memory>

namespace sky::gles {

    class Device : public rhi::Device {
    public:
        Device() = default;
        ~Device() = default;

        template <typename T, typename Desc>
        inline std::shared_ptr<T> CreateDeviceObject(const Desc &des)
        {
            auto res = new T(*this);
            if (!res->Init(des)) {
                delete res;
                res = nullptr;
            }
            return std::shared_ptr<T>(res);
        }

        bool Init(const Descriptor &desc);

        Context *GetMainContext() const;
        Context *GetGraphicsContext() const;
        Context *GetTransferContext() const;

        // Device Object
        CREATE_DEV_OBJ(SwapChain)

    private:
        std::unique_ptr<PBuffer> pBuffer;
        std::unique_ptr<Context> mainContext;
        std::unique_ptr<Context> graphicsContext;
        std::unique_ptr<Context> transferContext;
    };

}
