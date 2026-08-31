//
// Created on 2026/04/02.
//

#include <MetalDevice.h>
#include <MetalInstance.h>
#include <MetalSync.h>
#include <MetalBuffer.h>
#include <MetalImage.h>
#include <MetalSampler.h>
#include <MetalShader.h>
#include <MetalPipelineState.h>
#include <MetalSwapChain.h>
#include <MetalCommandPool.h>
#include <MetalUtils.h>
#include <core/logger/Logger.h>

#include <rdg/MetalDeviceFrameContext.h>

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

static const char *TAG = "AuroraMetal";

namespace sky::aurora {

    MetalThreadContext::~MetalThreadContext()
    {
        OnDetach();
    }

    void MetalThreadContext::OnAttach(uint32_t threadIndex)
    {
        (void)threadIndex;
        if (autoReleasePool == nullptr) {
            autoReleasePool = [[NSAutoreleasePool alloc] init];
        }
    }

    void MetalThreadContext::OnDetach()
    {
        if (autoReleasePool != nullptr) {
            [(NSAutoreleasePool *)autoReleasePool drain];
            autoReleasePool = nullptr;
        }
    }

    MetalDevice::MetalDevice(MetalInstance &inst)
        : instance(inst)
    {
    }

    MetalDevice::~MetalDevice()
    {
        for (auto &q : queues) { q.reset(); }
        if (metalDevice != nullptr) {
            [(id<MTLDevice>)metalDevice release];
            metalDevice = nullptr;
        }
    }

    bool MetalDevice::OnInit(const DeviceInit &init)
    {
        (void)init;

        auto *device = (id<MTLDevice>)instance.GetNativeDevice();
        if (device == nil) {
            LOG_E(TAG, "instance does not provide a Metal device");
            return false;
        }

        [device retain];
        metalDevice = device;

        for (size_t i = 0; i < queues.size(); ++i) {
            id<MTLCommandQueue> q = [device newCommandQueue];
            if (q == nil) {
                LOG_E(TAG, "failed to create Metal command queue %zu", i);
                return false;
            }
            queues[i] = std::make_unique<MetalQueue>(*this, static_cast<QueueType>(i),
                (__bridge_retained void *)q);
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
        capability.maxThreads = std::max(std::thread::hardware_concurrency(), 1U);
        capability.anisotropyEnable = true;
    }

    std::string MetalDevice::GetDeviceInfo() const
    {
        auto *device = (id<MTLDevice>)metalDevice;
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

    Fence *MetalDevice::CreateFence(const Fence::Descriptor &desc)
    {
        auto *fence = new MetalFence(*this);
        if (!fence->Init(desc)) {
            delete fence;
            return nullptr;
        }
        return fence;
    }

    Semaphore *MetalDevice::CreateSema(const Semaphore::Descriptor &desc)
    {
        auto *semaphore = new MetalSemaphore(*this);
        if (!semaphore->Init(desc)) {
            delete semaphore;
            return nullptr;
        }
        return semaphore;
    }

    Buffer *MetalDevice::CreateBuffer(const Buffer::Descriptor &desc)
    {
        auto *buffer = new MetalBuffer(*this);
        if (!buffer->Init(desc)) {
            delete buffer;
            return nullptr;
        }
        return buffer;
    }

    Image *MetalDevice::CreateImage(const Image::Descriptor &desc)
    {
        auto *image = new MetalImage(*this);
        if (!image->Init(desc)) {
            delete image;
            return nullptr;
        }
        return image;
    }

    Sampler *MetalDevice::CreateSampler(const Sampler::Descriptor &desc)
    {
        auto *sampler = new MetalSampler(*this);
        if (!sampler->Init(desc)) {
            delete sampler;
            return nullptr;
        }
        return sampler;
    }

    SwapChain *MetalDevice::CreateSwapChain(const SwapChain::Descriptor &desc)
    {
        auto *swapChain = new MetalSwapChain(*this);
        if (!swapChain->Init(desc)) {
            delete swapChain;
            return nullptr;
        }
        return swapChain;
    }

    ShaderFunction *MetalDevice::CreateShaderFunction(const ShaderFunction::Descriptor &desc)
    {
        auto *function = new MetalShaderFunction(*this);
        if (!function->Init(desc)) {
            delete function;
            return nullptr;
        }
        return function;
    }

    Shader *MetalDevice::CreateShader(const Shader::Descriptor &desc)
    {
        auto *shader = new MetalShader(*this);
        if (!shader->Init(desc)) {
            delete shader;
            return nullptr;
        }
        return shader;
    }

    GraphicsPipeline *MetalDevice::CreatePipelineState(const GraphicsPipeline::Descriptor &desc)
    {
        auto *pipeline = new MetalGraphicsPipeline(*this);
        if (!pipeline->Init(desc)) {
            delete pipeline;
            return nullptr;
        }
        return pipeline;
    }

    ComputePipeline *MetalDevice::CreatePipelineState(const ComputePipeline::Descriptor &desc)
    {
        auto *pipeline = new MetalComputePipeline(*this);
        if (!pipeline->Init(desc)) {
            delete pipeline;
            return nullptr;
        }
        return pipeline;
    }

    PixelFormatFeatureFlags MetalDevice::GetFormatFeatureFlags(PixelFormat format) const
    {
        const MTLPixelFormat mtlFormat = ToMetalPixelFormat(format);
        if (mtlFormat == MTLPixelFormatInvalid) {
            return {};
        }

        const auto &info = GetImageFormatInfo(format);
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
        const bool isInteger = (format == PixelFormat::R8_UINT ||
                                format == PixelFormat::R32_UINT ||
                                format == PixelFormat::RG32_UINT ||
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
        const bool isSrgb = (format == PixelFormat::R8_SRGB ||
                             format == PixelFormat::RGBA8_SRGB ||
                             format == PixelFormat::BGRA8_SRGB);
        const bool is3Channel = (format == PixelFormat::RGB32_SFLOAT ||
                                 format == PixelFormat::RGB32_UINT);

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
            case MTLPixelFormatRGBA8Unorm:
                result |= PixelFormatFeatureFlagBit::STORAGE;
                break;
            default:
                break;
            }
        }

        // atomic: R32Uint
        if (mtlFormat == MTLPixelFormatR32Uint) {
            result |= PixelFormatFeatureFlagBit::STORAGE_ATOMIC;
        }

        return result;
    }

    DeviceFrameContext* MetalDevice::CreateFrameContext(const DeviceFrameContextInitInfo& info)
    {
        return new MetalDeviceFrameContext(this, info);
    }

} // namespace sky::aurora
