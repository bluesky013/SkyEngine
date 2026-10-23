//
// Created on 2026/04/02.
//

#include <MetalShader.h>

#include "MetalLimits.h"
#include "MetalMSLSlots.h"
#include "MetalUtils.h"
#include <MetalDevice.h>
#include <core/logger/Logger.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <set>

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
        std::string message;
        if (!InitArtifact(desc, API::METAL, message)) {
            LOG_E(TAG, "%s", message.c_str());
            return false;
        }
        const auto &bytes = artifact->data;
        if (@available(macOS 11.0, iOS 14.0, *)) {
        } else {
            LOG_E(TAG, "Metal 2.3 shader artifacts require macOS 11 or iOS 14");
            return false;
        }
        auto *metalDevice = (__bridge id<MTLDevice>)device.GetNativeDevice();
        if (metalDevice == nil) {
            LOG_E(TAG, "invalid Metal device for shader creation");
            return false;
        }

        NSError       *error            = nil;
        id<MTLLibrary> metalLibrary     = nil;
        const auto     format           = artifact->identity.environment.codeFormat;
        const bool     isMetallib       = format == ShaderCodeFormat::METALLIB;
        const bool     hasMetallibMagic = bytes.size() >= 4 && memcmp(bytes.data(), "MTLB", 4) == 0;
        if ((format != ShaderCodeFormat::MSL_SOURCE && !isMetallib) || isMetallib != hasMetallibMagic) {
            LOG_E(TAG, "Metal payload does not match its explicit artifact code format");
            return false;
        }
        if (isMetallib) {
            dispatch_data_t data = dispatch_data_create(bytes.data(), bytes.size(), nullptr, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
            metalLibrary         = [metalDevice newLibraryWithData:data error:&error];
        } else {
            NSString *source = [[NSString alloc] initWithBytes:bytes.data() length:bytes.size() encoding:NSUTF8StringEncoding];
            if (source == nil) {
                LOG_E(TAG, "failed to decode MSL source");
                return false;
            }
            MTLCompileOptions *options = [[MTLCompileOptions alloc] init];
            if (@available(macOS 11.0, iOS 14.0, *))
                options.languageVersion = MTLLanguageVersion2_3;
            metalLibrary = [metalDevice newLibraryWithSource:source options:options error:&error];
        }
        if (metalLibrary == nil) {
            const char *message = error != nil ? [[error localizedDescription] UTF8String] : "unknown";
            LOG_E(TAG, "library creation failed: %s", message);
            return false;
        }

        NSString *entryName     = [NSString stringWithUTF8String:artifact->identity.nativeEntry.c_str()];
        auto     *metalFunction = [metalLibrary newFunctionWithName:entryName];
        if (metalFunction == nil) {
            LOG_E(TAG, "failed to find Metal entry point '%s'", artifact->identity.nativeEntry.c_str());
            return false;
        }
        const auto nativeType = [metalFunction functionType];
        if ((desc.stage != ShaderStageFlagBit::VS || nativeType != MTLFunctionTypeVertex) &&
            (desc.stage != ShaderStageFlagBit::FS || nativeType != MTLFunctionTypeFragment) &&
            (desc.stage != ShaderStageFlagBit::CS || nativeType != MTLFunctionTypeKernel)) {
            LOG_E(TAG, "Metal entry point has an incompatible shader stage");
            return false;
        }
        stage    = desc.stage;
        library  = (__bridge_retained void *)metalLibrary;
        function = (__bridge_retained void *)metalFunction;
        return true;
    }

    bool MetalShaderFunction::InitSpecialization(const MetalShaderFunction &source, const ShaderSpecialization &values)
    {
        @autoreleasepool {
            if (source.GetDevice() != &device || !source.library || !source.function)
                return false;
            const auto                      nativeSource = (__bridge id<MTLFunction>)source.function;
            std::map<uint32_t, MTLDataType> types;
            for (MTLFunctionConstant *constant in nativeSource.functionConstantsDictionary.allValues)
                types.emplace(static_cast<uint32_t>(constant.index), constant.type);
            auto *constants = [[MTLFunctionConstantValues alloc] init];
            for (const auto &entry : values.entries) {
                const auto type = types.find(entry.id);
                if (type == types.end())
                    continue;
                switch (type->second) {
                case MTLDataTypeBool: {
                    const bool value = entry.value != 0;
                    [constants setConstantValue:&value type:MTLDataTypeBool atIndex:entry.id];
                    break;
                }
                case MTLDataTypeInt: {
                    int32_t value;
                    std::memcpy(&value, &entry.value, sizeof(value));
                    [constants setConstantValue:&value type:MTLDataTypeInt atIndex:entry.id];
                    break;
                }
                case MTLDataTypeUInt: {
                    const uint32_t value = entry.value;
                    [constants setConstantValue:&value type:MTLDataTypeUInt atIndex:entry.id];
                    break;
                }
                case MTLDataTypeFloat: {
                    float value;
                    std::memcpy(&value, &entry.value, sizeof(value));
                    [constants setConstantValue:&value type:MTLDataTypeFloat atIndex:entry.id];
                    break;
                }
                default: return false;
                }
            }
            const auto nativeLibrary = (__bridge id<MTLLibrary>)source.library;
            NSError   *error         = nil;
            const auto native        = [nativeLibrary newFunctionWithName:nativeSource.name constantValues:constants error:&error];
            if (!native || native.functionType != nativeSource.functionType) {
                LOG_E(TAG, "Metal function specialization failed: %s", error ? error.localizedDescription.UTF8String : "invalid native function");
                return false;
            }
            artifact = source.GetArtifact();
            stage    = source.stage;
            library  = (__bridge_retained void *)nativeLibrary;
            function = (__bridge_retained void *)native;
            return true;
        }
    }

    bool MetalShader::SpecializeFunctions()
    {
        @autoreleasepool {
            std::map<uint32_t, MTLDataType> types;
            for (const auto *source : {vertexFunction.Get(), fragmentFunction.Get(), computeFunction.Get()}) {
                if (!source)
                    continue;
                const auto native = (__bridge id<MTLFunction>)source->GetNativeHandle();
                for (MTLFunctionConstant *constant in native.functionConstantsDictionary.allValues) {
                    if (constant.index > UINT32_MAX) {
                        LOG_E(TAG, "Metal function constant index exceeds the specialization protocol");
                        return false;
                    }
                    const auto [found, inserted] = types.emplace(static_cast<uint32_t>(constant.index), constant.type);
                    if (!inserted && found->second != constant.type) {
                        LOG_E(TAG, "Metal function constant has incompatible types across stages");
                        return false;
                    }
                }
            }
            std::set<uint32_t> supplied;
            for (const auto &entry : specialization.entries) {
                const auto type = types.find(entry.id);
                if (!supplied.insert(entry.id).second || type == types.end()) {
                    LOG_E(TAG, "Unknown or duplicate Metal specialization constant");
                    return false;
                }
                if ((type->second == MTLDataTypeBool && entry.value > 1) || (type->second != MTLDataTypeBool && type->second != MTLDataTypeInt &&
                                                                             type->second != MTLDataTypeUInt && type->second != MTLDataTypeFloat)) {
                    LOG_E(TAG, "Unsupported Metal specialization constant type or value");
                    return false;
                }
            }
            const auto specialize = [&](CounterPtr<MetalShaderFunction> &source) {
                if (!source)
                    return true;
                const auto native = (__bridge id<MTLFunction>)source->GetNativeHandle();
                if (native.functionConstantsDictionary.count == 0)
                    return true;
                CounterPtr<MetalShaderFunction> prepared = new MetalShaderFunction(device);
                if (!prepared->InitSpecialization(*source, specialization))
                    return false;
                source = std::move(prepared);
                return true;
            };
            return specialize(vertexFunction) && specialize(fragmentFunction) && specialize(computeFunction);
        }
    }

    const Device *MetalShaderFunction::GetDevice() const noexcept
    {
        return &device;
    }

    const Device *MetalShader::GetDevice() const noexcept
    {
        return &device;
    }

    MetalShader::MetalShader(MetalDevice &dev) : device(dev)
    {
    }

    bool MetalShader::Init(const Descriptor &desc)
    {
        std::string message;
        if (!BuildShaderProgramReflection(desc, device, reflection, message)) {
            LOG_E(TAG, "%s", message.c_str());
            return false;
        }
        if (!InitParameterTable(desc, message)) {
            LOG_E(TAG, "%s", message.c_str());
            return false;
        }
        if (desc.specialization != nullptr) {
            specialization = *desc.specialization;
        }

        for (const auto &range : reflection.pushConstants) {
            if (range.size > UINT32_MAX - range.offset) {
                LOG_E(TAG, "invalid push constant range");
                return false;
            }
            pushConstantSize = std::max(pushConstantSize, range.offset + range.size);
        }
        if (pushConstantSize != 0 && !reflection.nativeBindings.empty()) {
            LOG_E(TAG, "reflected native push constant slots are not implemented");
            return false;
        }
        MetalMSLBindingPlan bindingPlan;
        if (!BuildMetalMSLBindingPlan(reflection, bindingPlan, message)) {
            LOG_E(TAG, "%s", message.c_str());
            return false;
        }

        // Descriptor::vs/ps and ::cs alias the same union storage; the graphics
        // case is the one with a fragment shader (same convention as Vulkan).
        if (desc.ps != nullptr) {
            if (desc.vs == nullptr || desc.vs->GetDevice() != &device || desc.ps->GetDevice() != &device) {
                LOG_E(TAG, "graphics functions must belong to the shader device");
                return false;
            }
            vertexFunction   = static_cast<MetalShaderFunction *>(desc.vs);
            fragmentFunction = static_cast<MetalShaderFunction *>(desc.ps);
            if (vertexFunction->GetStage() != ShaderStageFlagBit::VS || fragmentFunction->GetStage() != ShaderStageFlagBit::FS) {
                LOG_E(TAG, "graphics functions have incompatible stages");
                return false;
            }
            const auto &vertex   = bindingPlan.stages[0];
            const auto &fragment = bindingPlan.stages[1];
            if (vertex.bufferCount > METAL_VERTEX_BUFFER_SLOT_BASE || fragment.bufferCount > METAL_MAX_BUFFER_SLOTS) {
                LOG_E(TAG, "invalid graphics resource slots or vertex buffer slot collision");
                return false;
            }
            bufferSlotCount  = vertex.bufferCount;
            pushConstantSlot = vertex.PushConstantSlot();
            resourceSetPlans = std::move(bindingPlan.sets);
            return SpecializeFunctions();
        }
        if (desc.cs == nullptr || desc.cs->GetDevice() != &device) {
            LOG_E(TAG, "compute function must belong to the shader device");
            return false;
        }
        computeFunction     = static_cast<MetalShaderFunction *>(desc.cs);
        const auto &compute = bindingPlan.stages[2];
        if (computeFunction->GetStage() != ShaderStageFlagBit::CS || compute.bufferCount > METAL_MAX_BUFFER_SLOTS) {
            LOG_E(TAG, "invalid compute function or resource slots");
            return false;
        }
        bufferSlotCount  = compute.bufferCount;
        pushConstantSlot = compute.PushConstantSlot();
        resourceSetPlans = std::move(bindingPlan.sets);
        return SpecializeFunctions();
    }

} // namespace sky::aurora
