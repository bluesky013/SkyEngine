//
// Created on 2026/04/02.
//

#include <MetalShader.h>
#include <MetalDevice.h>
#include <MetalUtils.h>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraMetal";

namespace sky::aurora {

    MetalShaderFunction::MetalShaderFunction(MetalDevice &dev)
        : device(dev)
    {
    }

    MetalShaderFunction::~MetalShaderFunction()
    {
        if (function != nullptr) {
            [(id<MTLFunction>)function release];
            function = nullptr;
        }
        if (library != nullptr) {
            [(id<MTLLibrary>)library release];
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

        auto *metalDevice = (id<MTLDevice>)device.GetNativeDevice();
        if (metalDevice == nil) {
            LOG_E(TAG, "invalid Metal device for shader creation");
            return false;
        }

        const auto &binary = binaryProvider->binaryData;
        NSString *source = [[NSString alloc] initWithBytes:binary->Data()
                                                   length:binary->Size()
                                                 encoding:NSUTF8StringEncoding];
        if (source == nil) {
            LOG_E(TAG, "failed to decode MSL source");
            return false;
        }

        NSError *error = nil;
        auto *metalLibrary = [metalDevice newLibraryWithSource:source options:nil error:&error];
        [source release];
        if (metalLibrary == nil) {
            const char *message = error != nil ? [[error localizedDescription] UTF8String] : "unknown";
            LOG_E(TAG, "newLibraryWithSource failed: %s", message);
            return false;
        }

        stage = desc.stage;
        // slang keeps the source-level entry name in MSL (e.g. mainVS); fall
        // back to the legacy VSMain/FSMain/CSMain convention for hand-written MSL
        NSString *entryName = !desc.entry.empty()
            ? [NSString stringWithUTF8String:desc.entry.c_str()]
            : ToMetalEntryPoint(desc.stage);
        auto *metalFunction = [metalLibrary newFunctionWithName:entryName];
        if (metalFunction == nil) {
            LOG_E(TAG, "failed to find Metal entry point '%s'",
                  desc.entry.empty() ? [ToMetalEntryPoint(desc.stage) UTF8String] : desc.entry.c_str());
            [metalLibrary release];
            return false;
        }

        library = metalLibrary;
        function = metalFunction;
        return true;
    }

    MetalShader::MetalShader(MetalDevice &dev)
        : device(dev)
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
        // indices in declaration order; by convention the push constant block is
        // declared last, so it owns the highest buffer slot.
        uint32_t bufferSlots = 0;
        for (const auto &res : reflection.resources) {
            switch (res.type) {
            case ShaderResourceType::UNIFORM_BUFFER:
            case ShaderResourceType::STORAGE_BUFFER:
            case ShaderResourceType::UNIFORM_BUFFER_DYNAMIC:
            case ShaderResourceType::STORAGE_BUFFER_DYNAMIC:
                bufferSlots = res.binding + res.count > bufferSlots ? res.binding + res.count : bufferSlots;
                break;
            default:
                break;
            }
        }
        pushConstantSlot = bufferSlots > 0 ? bufferSlots - 1 : 0;

        if (desc.cs != nullptr) {
            computeFunction = static_cast<MetalShaderFunction *>(desc.cs);
            return computeFunction != nullptr;
        }

        if (desc.vs != nullptr) {
            vertexFunction = static_cast<MetalShaderFunction *>(desc.vs);
        }
        if (desc.ps != nullptr) {
            fragmentFunction = static_cast<MetalShaderFunction *>(desc.ps);
        }
        return vertexFunction != nullptr || fragmentFunction != nullptr;
    }

} // namespace sky::aurora