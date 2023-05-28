//
// Created by Zach Lee on 2023/5/27.
//

#pragma once

#include <rhi/Core.h>
#import <Metal/Metal.h>

namespace sky::mtl {

    MTLPixelFormat FromRHI(rhi::PixelFormat format);
    MTLVertexFormat FromRHI(rhi::Format format);
    MTLStorageMode FromRHI(rhi::ImageUsageFlags usage, rhi::MemoryType memory);
    MTLSamplerAddressMode FromRHI(rhi::WrapMode mode);
    MTLSamplerMinMagFilter FromRHI(rhi::Filter filter);
    MTLSamplerMipFilter FromRHI(rhi::MipFilter filter);
    MTLCompareFunction FromRHI(rhi::CompareOp op);
    MTLStencilOperation FromRHI(rhi::StencilOp op);
    MTLWinding FromRHI(rhi::FrontFace mode);
    MTLCullMode FromRHI(rhi::CullingModeFlags mode);
    MTLTriangleFillMode FromRHI(rhi::PolygonMode mode);


} // namespace sky::mtl
