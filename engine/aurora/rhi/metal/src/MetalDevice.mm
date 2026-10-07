//
// Created on 2026/04/02.
//

#include "MetalBuffer.h"
#include "MetalCommandPool.h"
#include "MetalDescriptorBatch.h"
#include "MetalDevice.h"
#include "MetalImage.h"
#include "MetalInstance.h"
#include "MetalPipelineState.h"
#include "MetalResourceGroup.h"
#include "MetalSampler.h"
#include "MetalShader.h"
#include "MetalSwapChain.h"
#include "MetalSync.h"
#include "MetalUtils.h"
#include <core/logger/Logger.h>

#include "MetalBlitHelper.h"
#include "rdg/MetalRDGBackend.h"
#include <rdg/MetalDeviceFrameContext.h>

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

static const char *TAG = "AuroraMetal";

namespace sky::aurora {

    namespace {
        // Shared create-then-init pattern for every RHI object this device owns.
        template <typename T, typename D>
        T *CreateAndInit(MetalDevice &device, const D &desc)
        {
            auto *object = new T(device);
            if (!object->Init(desc)) {
                delete object;
                return nullptr;
            }
            return object;
        }
    } // namespace

    MetalThreadContext::~MetalThreadContext()
    {
        OnDetach();
    }

    void MetalThreadContext::OnAttach(uint32_t threadIndex)
    {
        (void)threadIndex;
    }

    void MetalThreadContext::OnDetach()
    {
    }

    MetalDevice::MetalDevice(MetalInstance &inst) : instance(inst)
    {
    }

    MetalDevice::~MetalDevice()
    {
        for (auto &q : queues) {
            q.reset();
        }
        if (metalDevice != nullptr) {
            (void)(__bridge_transfer id<MTLDevice>)metalDevice;
            metalDevice = nullptr;
        }
    }

    bool MetalDevice::OnInit(const DeviceInit &init)
    {
        (void)init;

        auto *device = (__bridge id<MTLDevice>)instance.GetNativeDevice();
        if (device == nil) {
            LOG_E(TAG, "instance does not provide a Metal device");
            return false;
        }

        metalDevice = (__bridge_retained void *)device;
        blitHelper  = std::make_unique<MetalBlitHelper>(*this);

        for (size_t i = 0; i < queues.size(); ++i) {
            id<MTLCommandQueue> q = [device newCommandQueue];
            if (q == nil) {
                LOG_E(TAG, "failed to create Metal command queue %zu", i);
                return false;
            }
            queues[i] = std::make_unique<MetalQueue>(*this, static_cast<QueueType>(i), (__bridge_retained void *)q);
        }

        LOG_I(TAG, "Metal device initialized: %s", [[device name] UTF8String]);
        return true;
    }

    Queue *MetalDevice::GetQueue(QueueType type)
    {
        return queues[static_cast<size_t>(type)].get();
    }

    void *MetalDevice::GetCommandQueue() const
    {
        const auto &q = queues[static_cast<size_t>(QueueType::GRAPHICS)];
        return q ? q->GetNativeHandle() : nullptr;
    }

    void MetalDevice::UpdateDeviceCaps()
    {
        capability.maxThreads       = std::max(std::thread::hardware_concurrency(), 1U);
        capability.anisotropyEnable = true;

        auto *mtlDevice  = (__bridge id<MTLDevice>)metalDevice;
        capability.isUMA = mtlDevice != nil && [mtlDevice hasUnifiedMemory];
        // Metal guarantees 256-byte alignment for constant buffer offsets on
        // macOS; there is no MTLDevice query for it
        capability.minUniformBufferOffsetAlignment = 256u;

        if (mtlDevice != nil) {
            // object/mesh pipeline requires Apple7 (A15) / Mac2 class GPUs
            feature.meshShader = [mtlDevice supportsFamily:MTLGPUFamilyApple7] || [mtlDevice supportsFamily:MTLGPUFamilyMac2];
            // framebuffer fetch (programmable blending input) is an Apple-GPU feature
            feature.framebufferFetch = [mtlDevice supportsFamily:MTLGPUFamilyApple1];
            // MTLDraw*IndirectArguments carry baseInstance
            feature.firstInstanceIndirect = true;
            // tier2 bindless argument-buffer heap needs shader-side argument
            // buffer emission, which the slang MSL path does not produce yet
            feature.descriptorHeap     = false;
            feature.descriptorIndexing = false;
        }
    }

    std::string MetalDevice::GetDeviceInfo() const
    {
        auto *device = (__bridge id<MTLDevice>)metalDevice;
        if (device == nil) {
            return "Metal";
        }

        NSString *name = [device name];
        return name != nil ? std::string([name UTF8String]) : std::string("Metal");
    }

    void MetalDevice::WaitIdle() const
    {
        for (const auto &q : queues) {
            if (q) {
                const_cast<MetalQueue *>(q.get())->WaitIdle();
            }
        }
    }

    CommandPool *MetalDevice::CreateCommandPool(QueueType type)
    {
        auto *queue = queues[static_cast<size_t>(type)].get();
        if (queue == nullptr) {
            return nullptr;
        }
        auto *pool = new MetalCommandPool(*this, queue->GetNativeHandle());
        if (!pool->Init()) {
            delete pool;
            return nullptr;
        }
        return pool;
    }

    RDGBackend *MetalDevice::CreateRDGBackend()
    {
        return new MetalRDGBackend();
    }

    Fence *MetalDevice::CreateFence(const Fence::Descriptor &desc)
    {
        return CreateAndInit<MetalFence>(*this, desc);
    }

    Semaphore *MetalDevice::CreateSema(const Semaphore::Descriptor &desc)
    {
        return CreateAndInit<MetalSemaphore>(*this, desc);
    }

    Buffer *MetalDevice::CreateBuffer(const Buffer::Descriptor &desc)
    {
        return CreateAndInit<MetalBuffer>(*this, desc);
    }

    Image *MetalDevice::CreateImage(const Image::Descriptor &desc)
    {
        return CreateAndInit<MetalImage>(*this, desc);
    }

    Sampler *MetalDevice::CreateSampler(const Sampler::Descriptor &desc)
    {
        return CreateAndInit<MetalSampler>(*this, desc);
    }

    ResourceGroup *MetalDevice::CreateResourceGroup(const ResourceGroup::Descriptor &desc)
    {
        return CreateAndInit<MetalResourceGroup>(*this, desc);
    }

    DescriptorBatch *MetalDevice::CreateDescriptorBatch()
    {
        return new MetalDescriptorBatch();
    }

    SwapChain *MetalDevice::CreateSwapChain(const SwapChain::Descriptor &desc)
    {
        return CreateAndInit<MetalSwapChain>(*this, desc);
    }

    ShaderFunction *MetalDevice::CreateShaderFunction(const ShaderFunction::Descriptor &desc)
    {
        return CreateAndInit<MetalShaderFunction>(*this, desc);
    }

    Shader *MetalDevice::CreateShader(const Shader::Descriptor &desc)
    {
        return CreateAndInit<MetalShader>(*this, desc);
    }

    GraphicsPipeline *MetalDevice::CreatePipelineState(const GraphicsPipeline::Descriptor &desc)
    {
        return CreateAndInit<MetalGraphicsPipeline>(*this, desc);
    }

    ComputePipeline *MetalDevice::CreatePipelineState(const ComputePipeline::Descriptor &desc)
    {
        return CreateAndInit<MetalComputePipeline>(*this, desc);
    }

    PixelFormatFeatureFlags MetalDevice::GetFormatFeatureFlags(PixelFormat format) const
    {
        const MTLPixelFormat mtlFormat = ToMetalPixelFormat(format);
        if (mtlFormat == MTLPixelFormatInvalid) {
            return {};
        }

        const auto             &info = GetImageFormatInfo(format);
        PixelFormatFeatureFlags result;

        if (info.isCompressed) {
            result |= PixelFormatFeatureFlagBit::SAMPLE;
            result |= PixelFormatFeatureFlagBit::SAMPLE_FILTER;
            return result;
        }

        if (info.hasDepth || info.hasStencil) {
            result |= PixelFormatFeatureFlagBit::DEPTH_STENCIL;
            result |= PixelFormatFeatureFlagBit::SAMPLE;
            return result;
        }

        // all valid non-compressed non-depth Metal formats are sampleable
        result |= PixelFormatFeatureFlagBit::SAMPLE;

        // integer formats: no blend, no filter
        const bool isInteger = (format == PixelFormat::R8_UINT || format == PixelFormat::R32_UINT || format == PixelFormat::RG32_UINT ||
                                format == PixelFormat::RGBA32_UINT);

        if (!isInteger) {
            result |= PixelFormatFeatureFlagBit::SAMPLE_FILTER;
        }

        // color attachment: all non-compressed non-depth valid formats
        result |= PixelFormatFeatureFlagBit::COLOR;

        if (!isInteger) {
            result |= PixelFormatFeatureFlagBit::BLEND;
        }

        // storage: Metal supports write for most non-compressed formats
        // (8-bit, 16-bit, 32-bit; not sRGB, not 3-channel)
        const bool isSrgb     = (format == PixelFormat::R8_SRGB || format == PixelFormat::RGBA8_SRGB || format == PixelFormat::BGRA8_SRGB);
        const bool is3Channel = (format == PixelFormat::RGB32_SFLOAT || format == PixelFormat::RGB32_UINT);

        if (!isSrgb && !is3Channel) {
            switch (mtlFormat) {
            case MTLPixelFormatR8Unorm:
            case MTLPixelFormatR8Uint:
            case MTLPixelFormatR16Float:
            case MTLPixelFormatRG16Float:
            case MTLPixelFormatRGBA16Float:
            case MTLPixelFormatR32Float:
            case MTLPixelFormatRG32Float:
            case MTLPixelFormatRGBA32Float:
            case MTLPixelFormatR32Uint:
            case MTLPixelFormatRG32Uint:
            case MTLPixelFormatRGBA32Uint:
            case MTLPixelFormatRGBA8Unorm: result |= PixelFormatFeatureFlagBit::STORAGE; break;
            default: break;
            }
        }

        // atomic: R32Uint
        if (mtlFormat == MTLPixelFormatR32Uint) {
            result |= PixelFormatFeatureFlagBit::STORAGE_ATOMIC;
        }

        return result;
    }

    DeviceFrameContext *MetalDevice::CreateFrameContext(const DeviceFrameContextInitInfo &info)
    {
        return new MetalDeviceFrameContext(this, info);
    }

} // namespace sky::aurora
