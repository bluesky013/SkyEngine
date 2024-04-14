//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

#include <string>
#include <memory>
#include <core/util/DynamicModule.h>
#include <rhi/Device.h>

namespace sky::rhi {
#ifdef SKY_ENABLE_XR
    class XRInterface;
#endif

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
#ifdef SKY_ENABLE_XR
            XRInterface *xrInterface = nullptr;
#endif
        };

        static Instance *Create(const Descriptor &);
        static void    Destroy(Instance *);

        virtual Device *CreateDevice(const Device::Descriptor &desc) { return nullptr; }
        virtual bool Init(const Descriptor &) { return false; }

#ifdef SKY_ENABLE_XR
        XRInterface *GetXRInterface() { return xrInterface; }
#endif
    protected:
        Instance() = default;
        virtual ~Instance() = default;

#ifdef SKY_ENABLE_XR
        XRInterface *xrInterface;
#endif
    };

    API GetApiByString(const std::string &str);

}
