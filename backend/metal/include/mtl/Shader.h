//
// Created by Zach Lee on 2023/5/28.
//

#pragma once

#include <rhi/Shader.h>
#include <mtl/DevObject.h>
#import <Metal/MTLLibrary.h>

namespace sky::mtl {
    class Device;

    class Shader : public rhi::Shader, public DevObject {
    public:
        Shader(Device &dev) : DevObject(dev) {}
        ~Shader();

        id<MTLFunction> GetNativeHandle() const { return function; }

    private:
        bool Init(const Descriptor &desc);

        id<MTLFunction> function = nil;
    };

} // namespace sky::mtl
