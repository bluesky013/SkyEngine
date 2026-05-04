//
// Created on 2026/04/07.
//

#include <GLESCommandPool.h>
#include <GLESDevice.h>
#include <GLESEncoder.h>
#include <GLESLoader.h>
#include <GLESConversion.h>

namespace sky::aurora {

    // ---- GLESCommandBuffer ----

    GLESCommandBuffer::GLESCommandBuffer(GLESDevice &device)
        : device(device)
    {
    }

    void GLESCommandBuffer::Begin()
    {
        // GLES is immediate mode, no command buffer recording concept.
    }

    void GLESCommandBuffer::End()
    {
        // GLES is immediate mode, no command buffer recording concept.
    }

    void GLESCommandBuffer::PipelineBarrier(const BarrierInfo &info)
    {
        GLbitfield bits = 0;
        auto accumulate = [&](const AccessFlags &access) {
            bits |= AccessFlagsToGLBarrierBits(access);
        };
        for (const auto &m  : info.memoryBarriers) { accumulate(m.srcAccess); accumulate(m.dstAccess); }
        for (const auto &bb : info.bufferBarriers) { accumulate(bb.srcAccess); accumulate(bb.dstAccess); }
        for (const auto &ib : info.imageBarriers)  { accumulate(ib.srcAccess); accumulate(ib.dstAccess); }

        if (bits == 0) {
            return;
        }
        glMemoryBarrier(bits);
    }

    std::unique_ptr<GraphicsEncoder> GLESCommandBuffer::CreateGraphicsEncoder()
    {
        return std::make_unique<GLESGraphicsEncoder>(device);
    }

    std::unique_ptr<ComputeEncoder> GLESCommandBuffer::CreateComputeEncoder()
    {
        return std::make_unique<GLESComputeEncoder>(device);
    }

    std::unique_ptr<BlitEncoder> GLESCommandBuffer::CreateBlitEncoder()
    {
        return std::make_unique<GLESBlitEncoder>(device);
    }

    // ---- GLESCommandPool ----

    GLESCommandPool::GLESCommandPool(GLESDevice &device)
        : device(device)
    {
    }

    GLESCommandPool::~GLESCommandPool()
    {
        for (auto *buffer : allocatedBuffers) {
            delete buffer;
        }
        allocatedBuffers.clear();
    }

    bool GLESCommandPool::Init()
    {
        return true;
    }

    void GLESCommandPool::Reset()
    {
    }

    CommandBuffer *GLESCommandPool::Allocate()
    {
        auto *cmdBuffer = new GLESCommandBuffer(device);
        allocatedBuffers.push_back(cmdBuffer);
        return cmdBuffer;
    }

} // namespace sky::aurora
