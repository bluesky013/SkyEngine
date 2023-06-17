//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

#include <string>
#include <memory>
#include <core/util/DynamicModule.h>
#include <rhi/Device.h>

namespace sky::rhi {
    enum class API {
        DEFAULT = 0,
        VULKAN,
        METAL,
        DX12,
        GLES
    };

    class Instance {
    public:
        struct Descriptor {
            std::string appName;
            std::string engineName;
            bool        enableDebugLayer;
            API         api;
        };

        static Instance *Create(const Descriptor &);
        static void    Destroy(Instance *);

        virtual Device *CreateDevice(const Device::Descriptor &desc) { return nullptr; }
        virtual bool Init(const Descriptor &) { return false; }
    protected:
        Instance() = default;
        virtual ~Instance() = default;
    };

}
