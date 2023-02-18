//
// Created by Zach Lee on 2022/11/4.
//

#pragma once

#include <string>
#include <vector>
#include <metal/Device.h>

namespace MTL {
    class Device;
}

namespace sky::mtl {
    class Instance {
    public:
        struct Descriptor {
            std::string appName;
            std::string engineName;
            bool        enableDebugLayer;
        };

        static Instance *Create(const Descriptor &);
        static void    Destroy(Instance *);

        Device *CreateDevice(const Device::Descriptor &);

        const std::vector<MTL::Device*> &GetMtlDevices() const;

    private:
        Instance() = default;
        ~Instance() = default;

        bool Init(const Descriptor &);
        std::vector<MTL::Device*> devices;
    };

}
