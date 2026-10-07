//
// Created on 2026/04/02.
//

#include "MetalDevice.h"
#include "MetalMSLSlots.h"
#include "MetalShader.h"
#include "MetalUtils.h"
#include <core/logger/Logger.h>

#include <algorithm>
#include <cstring>

static const char *TAG = "AuroraMetal";

namespace sky::aurora {

    MetalShaderFunction::MetalShaderFunction(MetalDevice &dev) : device(dev)
    {
    }

    MetalShaderFunction::~MetalShaderFunction()
    {
        if (function != nullptr) {
            (void)(__bridge_transfer id<MTLFunction>)function;
            function = nullptr;
        }
        if (library != nullptr) {
            (void)(__bridge_transfer id<MTLLibrary>)library;
            library = nullptr;
        }
    }

    bool MetalShaderFunction::Init(const Descriptor &desc)
    {
        if (desc.data == nullptr) {
            LOG_E(TAG, "shader function requires shader data");
            return false;
        }

        const auto *binaryProvider = static_cast<const ShaderBinaryProvider *>(desc.data.Get());
        if (binaryProvider->binaryData == nullptr) {
            LOG_E(TAG, "shader function missing binary payload");
            return false;
        }

        auto *metalDevice = (__bridge id<MTLDevice>)device.GetNativeDevice();
        if (metalDevice == nil) {
            LOG_E(TAG, "invalid Metal device for shader creation");
            return false;
        }

        const auto    &binary       = binaryProvider->binaryData;
        NSError       *error        = nil;
        id<MTLLibrary> metalLibrary = nil;
        // pre-compiled .metallib blobs start with the "MTLB" magic; anything
        // else is treated as MSL source text and compiled at runtime
        const bool isMetallib = binary->Size() >= 4 && memcmp(binary->Data(), "MTLB", 4) == 0;
        if (isMetallib) {
            dispatch_data_t data = dispatch_data_create(binary->Data(), binary->Size(), nullptr, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
            metalLibrary         = [metalDevice newLibraryWithData:data error:&error];
        } else {
            NSString *source = [[NSString alloc] initWithBytes:binary->Data() length:binary->Size() encoding:NSUTF8StringEncoding];
            if (source == nil) {
                LOG_E(TAG, "failed to decode MSL source");
                return false;
            }
            metalLibrary = [metalDevice newLibraryWithSource:source options:nil error:&error];
        }
        if (metalLibrary == nil) {
            const char *message = error != nil ? [[error localizedDescription] UTF8String] : "unknown";
            LOG_E(TAG, "library creation failed: %s", message);
            return false;
        }

        stage = desc.stage;
        // slang keeps the source-level entry name in MSL (e.g. mainVS); fall
        // back to the legacy VSMain/FSMain/CSMain convention for hand-written MSL
        NSString *entryName     = !desc.entry.empty() ? [NSString stringWithUTF8String:desc.entry.c_str()] : ToMetalEntryPoint(desc.stage);
        auto     *metalFunction = [metalLibrary newFunctionWithName:entryName];
        if (metalFunction == nil) {
            LOG_E(TAG, "failed to find Metal entry point '%s'", desc.entry.empty() ? [ToMetalEntryPoint(desc.stage) UTF8String] : desc.entry.c_str());
            return false;
        }
        library  = (__bridge_retained void *)metalLibrary;
        function = (__bridge_retained void *)metalFunction;
        return true;
    }

    MetalShader::MetalShader(MetalDevice &dev) : device(dev)
    {
    }

    bool MetalShader::Init(const Descriptor &desc)
    {
        if (desc.reflection == nullptr) {
            LOG_E(TAG, "shader requires a non-null reflection");
            return false;
        }
        reflection = *desc.reflection;
        if (desc.specialization != nullptr) {
            specialization = *desc.specialization;
        }

        // slang MSL flattens buffer-kind resources to sequential [[buffer(N)]]
        // indices in declaration order; the push constant block is declared last
        // and owns the highest buffer slot (shared layout, see MetalMSLSlots.h)
        const auto layout = BuildMetalMSLSlotLayout(reflection, 0);
        pushConstantSlot  = layout.PushConstantSlot();
        bufferSlotCount   = layout.bufferCount;

        for (const auto &range : reflection.pushConstants) {
            pushConstantSize = std::max(pushConstantSize, range.offset + range.size);
        }

        // Descriptor::vs/ps and ::cs alias the same union storage; the graphics
        // case is the one with a fragment shader (same convention as Vulkan).
        if (desc.ps != nullptr) {
            vertexFunction   = desc.vs != nullptr ? static_cast<MetalShaderFunction *>(desc.vs) : nullptr;
            fragmentFunction = static_cast<MetalShaderFunction *>(desc.ps);
            return vertexFunction != nullptr || fragmentFunction != nullptr;
        }

        computeFunction = static_cast<MetalShaderFunction *>(desc.cs);
        return computeFunction != nullptr;
    }

} // namespace sky::aurora