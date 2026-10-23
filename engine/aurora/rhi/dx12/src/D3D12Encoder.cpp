//
// Created on 2026/04/07.
//

#include <D3D12Encoder.h>

#include <D3D12BlitHelper.h>
#include <D3D12Buffer.h>
#include <D3D12Conversion.h>
#include <D3D12DescriptorHeap.h>
#include <D3D12Device.h>
#include <D3D12Image.h>
#include <D3D12PipelineState.h>
#include <D3D12ResourceGroup.h>
#include <D3D12RootSignature.h>
#include <D3D12ShaderFunction.h>
#include <core/logger/Logger.h>
#include <core/platform/Platform.h>

namespace sky::aurora {

    static const char *TAG = "AuroraDX12";

    // ---- D3D12GraphicsEncoder ----

    D3D12GraphicsEncoder::D3D12GraphicsEncoder(D3D12Device &device, ID3D12GraphicsCommandList *cmdList) : device(device), cmdList(cmdList)
    {
    }

    void D3D12GraphicsEncoder::BeginRendering(const RenderingInfo &info)
    {
        D3D12_VIEWPORT vp = {};
        vp.TopLeftX       = static_cast<float>(info.renderArea.offset.x);
        vp.TopLeftY       = static_cast<float>(info.renderArea.offset.y);
        vp.Width          = static_cast<float>(info.renderArea.extent.width);
        vp.Height         = static_cast<float>(info.renderArea.extent.height);
        vp.MinDepth       = 0.f;
        vp.MaxDepth       = 1.f;
        cmdList->RSSetViewports(1, &vp);

        D3D12_RECT sc = {};
        sc.left       = info.renderArea.offset.x;
        sc.top        = info.renderArea.offset.y;
        sc.right      = info.renderArea.offset.x + static_cast<LONG>(info.renderArea.extent.width);
        sc.bottom     = info.renderArea.offset.y + static_cast<LONG>(info.renderArea.extent.height);
        cmdList->RSSetScissorRects(1, &sc);

        auto *allocator = device.GetDescriptorAllocator();
        if (allocator == nullptr) {
            return;
        }

        const ImageSubRange fullRange{};

        D3D12_CPU_DESCRIPTOR_HANDLE rtvs[MAX_COLOR_ATTACHMENTS] = {};
        const uint32_t              numColors                   = info.numColors < MAX_COLOR_ATTACHMENTS ? info.numColors : MAX_COLOR_ATTACHMENTS;
        uint32_t                    rtvFirst                    = 0;
        if (numColors > 0 && allocator->AllocateRtv(numColors, rtvFirst)) {
            for (uint32_t i = 0; i < numColors; ++i) {
                rtvs[i] = allocator->GetRtvCpuHandle(rtvFirst + i);
                if (info.colors[i].image != nullptr) {
                    static_cast<D3D12Image *>(info.colors[i].image)->CreateRTV(rtvs[i], fullRange);
                }
            }
        } else if (numColors > 0) {
            LOG_E(TAG, "out of RTV descriptors for %u color attachments", numColors);
        }

        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};
        bool                        hasDsv    = info.depthStencil.image != nullptr;
        if (hasDsv) {
            uint32_t dsvFirst = 0;
            if (allocator->AllocateDsv(dsvFirst)) {
                dsvHandle = allocator->GetDsvCpuHandle(dsvFirst);
                static_cast<D3D12Image *>(info.depthStencil.image)->CreateDSV(dsvHandle, fullRange);
            } else {
                LOG_E(TAG, "out of DSV descriptors");
                hasDsv = false;
            }
        }

        cmdList->OMSetRenderTargets(numColors, numColors > 0 ? rtvs : nullptr, FALSE, hasDsv ? &dsvHandle : nullptr);

        for (uint32_t i = 0; i < numColors; ++i) {
            if (info.colors[i].loadOp == LoadOp::CLEAR) {
                cmdList->ClearRenderTargetView(rtvs[i], info.colors[i].clearValue.color.float32, 0, nullptr);
            }
        }
        if (hasDsv) {
            const auto       &ds    = info.depthStencil;
            D3D12_CLEAR_FLAGS flags = {};
            if (ds.depthLoadOp == LoadOp::CLEAR) {
                flags |= D3D12_CLEAR_FLAG_DEPTH;
            }
            if (ds.stencilLoadOp == LoadOp::CLEAR) {
                flags |= D3D12_CLEAR_FLAG_STENCIL;
            }
            if (flags != 0) {
                cmdList->ClearDepthStencilView(dsvHandle, flags, ds.clearValue.depthStencil.depth,
                                               static_cast<UINT8>(ds.clearValue.depthStencil.stencil), 0, nullptr);
            }
        }
    }

    void D3D12GraphicsEncoder::EndRendering()
    {
        // No-op for D3D12 (no explicit render pass end)
    }

    void D3D12GraphicsEncoder::BindPipeline(GraphicsPipeline *pso)
    {
        auto *d3dPso         = static_cast<D3D12GraphicsPipeline *>(pso);
        currentRootSignature = d3dPso->GetRootSignature();
        currentVertexStrides = d3dPso->GetVertexStrides();
        cmdList->SetPipelineState(d3dPso->GetNativeHandle());
        if (currentRootSignature != nullptr) {
            // Root signature is a command-list state; it must be bound before any
            // root descriptor table (BindResourceGroup) is set.
            cmdList->SetGraphicsRootSignature(currentRootSignature->GetNativeHandle());
        }
        // Primitive topology is also command-list state on DX12.
        cmdList->IASetPrimitiveTopology(FromPrimitiveTopologyValue(d3dPso->GetPrimitiveTopology()));
    }

    void D3D12GraphicsEncoder::BindResourceGroup(uint32_t set, ResourceGroup *group, uint32_t numDynamicOffsets, const uint32_t *dynamicOffsets)
    {
        if (currentRootSignature == nullptr || group == nullptr || group->GetDevice() != &device) {
            return;
        }
        auto *d3dGroup = static_cast<D3D12ResourceGroup *>(group);
        if (group->GetSet() != set || !group->GetShader() ||
            static_cast<D3D12Shader *>(group->GetShader())->GetRootSignature() != currentRootSignature) {
            LOG_E(TAG, "ResourceGroup does not match the bound shader/set");
            return;
        }

        // copy staging -> current frame's shader-visible heap before binding
        d3dGroup->EnsureFrameCopy();

        auto *allocator = device.GetDescriptorAllocator();
        if (allocator == nullptr) {
            return;
        }
        ID3D12DescriptorHeap *heaps[2] = {allocator->GetCbvSrvUavHeap(), allocator->GetSamplerHeap()};
        cmdList->SetDescriptorHeaps(2, heaps);

        for (const auto &table : currentRootSignature->GetDescriptorTables(set)) {
            cmdList->SetGraphicsRootDescriptorTable(table.root, table.sampler ? d3dGroup->GetSamplerGpuHandle() : d3dGroup->GetCbvSrvUavGpuHandle());
        }

        // Each logical dynamic buffer may occupy distinct native roots across stages.
        const auto &dynamics = d3dGroup->GetDynamicBindings();
        for (size_t i = 0; i < dynamics.size(); ++i) {
            const auto &d = dynamics[i];
            if (d.buffer == nullptr) {
                continue;
            }
            if ((numDynamicOffsets && !dynamicOffsets) || d.buffer->GetDevice() != &device) {
                LOG_E("AuroraDX12", "Dynamic buffer offsets or owner are invalid");
                return;
            }
            const uint32_t offset = i < numDynamicOffsets ? dynamicOffsets[i] : 0;
            const auto     size   = d.buffer->GetSize();
            if (d.baseOffset >= size || offset > size - d.baseOffset || !d.range || d.range > size - d.baseOffset - offset) {
                LOG_E("AuroraDX12", "Dynamic buffer range exceeds its backing");
                return;
            }
            const auto                      effectiveOffset = d.baseOffset + offset;
            const D3D12_GPU_VIRTUAL_ADDRESS addr            = d.buffer->GetNativeHandle()->GetGPUVirtualAddress() + effectiveOffset;
            if (d.resource.type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC) {
                if (const auto error = d.buffer->CheckStorageRange(d.resource, effectiveOffset, d.range)) {
                    LOG_E("AuroraDX12", "%s", error->message.c_str());
                    return;
                }

            } else {
                if (effectiveOffset % 256 || ((d.range + 255) & ~uint64_t{255}) > size - effectiveOffset) {
                    LOG_E("AuroraDX12", "Dynamic uniform buffer alignment or backing is invalid");
                    return;
                }
            }
            for (const auto &mapping : currentRootSignature->GetDynamicBindings(set)) {
                if (mapping.binding != d.binding)
                    continue;
                switch (mapping.category) {
                case ShaderNativeCategory::CBV: cmdList->SetGraphicsRootConstantBufferView(mapping.root, addr); break;
                case ShaderNativeCategory::SRV: cmdList->SetGraphicsRootShaderResourceView(mapping.root, addr); break;
                case ShaderNativeCategory::UAV: cmdList->SetGraphicsRootUnorderedAccessView(mapping.root, addr); break;
                default: LOG_E(TAG, "Invalid dynamic native category"); return;
                }
            }
        }
    }

    void D3D12GraphicsEncoder::BindDescriptorHeap(DescriptorHeap *heap)
    {
        auto *d3dHeap = static_cast<D3D12DescriptorHeap *>(heap);
        if (d3dHeap == nullptr) {
            return;
        }
        ID3D12DescriptorHeap *heaps[2] = {};
        uint32_t              count    = 0;
        if (d3dHeap->GetResourceHeap() != nullptr) {
            heaps[count++] = d3dHeap->GetResourceHeap();
        }
        if (d3dHeap->GetSamplerHeap() != nullptr) {
            heaps[count++] = d3dHeap->GetSamplerHeap();
        }
        if (count > 0) {
            cmdList->SetDescriptorHeaps(count, heaps);
        }
    }

    void D3D12GraphicsEncoder::PushConstants(ShaderStageFlags /*stages*/, uint32_t offset, uint32_t size, const void *data)
    {
        if (currentRootSignature == nullptr) {
            return;
        }
        const uint32_t rootParam = currentRootSignature->GetPushConstantRootParam();
        if (rootParam == INVALID_INDEX) {
            return;
        }
        // D3D12 root constants are 32-bit granular; a non-aligned range cannot
        // be expressed. Reject instead of silently truncating.
        if ((offset % 4) != 0 || (size % 4) != 0) {
            LOG_E(TAG, "PushConstants requires 4-byte aligned offset/size on D3D12 (offset=%u size=%u)", offset, size);
            return;
        }
        cmdList->SetGraphicsRoot32BitConstants(rootParam, size / 4, data, offset / 4);
    }

    void D3D12GraphicsEncoder::BindVertexBuffers(uint32_t firstBinding, uint32_t count, const BufferView *views)
    {
        SKY_ASSERT(count <= MAX_VERTEX_BINDINGS);
        D3D12_VERTEX_BUFFER_VIEW vbViews[MAX_VERTEX_BINDINGS] = {};

        for (uint32_t i = 0; i < count; ++i) {
            auto *buf = static_cast<D3D12Buffer *>(views[i].buffer);
            if (buf == nullptr) {
                continue;
            }
            vbViews[i].BufferLocation = buf->GetNativeHandle()->GetGPUVirtualAddress() + views[i].offset;
            // BufferView::range == 0 means "rest of the buffer".
            const uint64_t totalSize = buf->GetSize();
            vbViews[i].SizeInBytes =
                static_cast<UINT>(views[i].range != 0 ? views[i].range : (totalSize > views[i].offset ? totalSize - views[i].offset : 0));
            const uint32_t slot      = firstBinding + i;
            vbViews[i].StrideInBytes = slot < currentVertexStrides.size() ? currentVertexStrides[slot] : 0;
        }
        cmdList->IASetVertexBuffers(firstBinding, count, vbViews);
    }

    void D3D12GraphicsEncoder::BindIndexBuffer(Buffer *buffer, uint64_t offset, IndexType type)
    {
        auto                   *buf    = static_cast<D3D12Buffer *>(buffer);
        D3D12_INDEX_BUFFER_VIEW ibView = {};
        ibView.BufferLocation          = buf->GetNativeHandle()->GetGPUVirtualAddress() + offset;
        ibView.Format                  = FromIndexType(type);
        const uint64_t totalSize       = buf->GetSize();
        ibView.SizeInBytes             = totalSize > offset ? static_cast<UINT>(totalSize - offset) : 0;
        cmdList->IASetIndexBuffer(&ibView);
    }

    void D3D12GraphicsEncoder::SetViewport(uint32_t count, const Viewport *viewports)
    {
        D3D12_VIEWPORT d3dViewports[16];
        uint32_t       n = count < 16 ? count : 16;
        for (uint32_t i = 0; i < n; ++i) {
            d3dViewports[i].TopLeftX = viewports[i].x;
            d3dViewports[i].TopLeftY = viewports[i].y;
            d3dViewports[i].Width    = viewports[i].width;
            d3dViewports[i].Height   = viewports[i].height;
            d3dViewports[i].MinDepth = viewports[i].minDepth;
            d3dViewports[i].MaxDepth = viewports[i].maxDepth;
        }
        cmdList->RSSetViewports(n, d3dViewports);
    }

    void D3D12GraphicsEncoder::SetScissor(uint32_t count, const Rect2D *scissors)
    {
        D3D12_RECT d3dRects[16];
        uint32_t   n = count < 16 ? count : 16;
        for (uint32_t i = 0; i < n; ++i) {
            d3dRects[i].left   = scissors[i].offset.x;
            d3dRects[i].top    = scissors[i].offset.y;
            d3dRects[i].right  = scissors[i].offset.x + static_cast<LONG>(scissors[i].extent.width);
            d3dRects[i].bottom = scissors[i].offset.y + static_cast<LONG>(scissors[i].extent.height);
        }
        cmdList->RSSetScissorRects(n, d3dRects);
    }

    void D3D12GraphicsEncoder::Draw(const CmdDrawLinear &cmd)
    {
        cmdList->DrawInstanced(cmd.vertexCount, cmd.instanceCount, cmd.firstVertex, cmd.firstInstance);
    }

    void D3D12GraphicsEncoder::DrawIndexed(const CmdDrawIndexed &cmd)
    {
        cmdList->DrawIndexedInstanced(cmd.indexCount, cmd.instanceCount, cmd.firstIndex, cmd.vertexOffset, cmd.firstInstance);
    }

    void D3D12GraphicsEncoder::DrawIndirect(Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t stride)
    {
        if (buffer == nullptr) {
            return;
        }
        ID3D12CommandSignature *signature = device.GetIndirectSignature(IndirectKind::DRAW, stride);
        if (signature == nullptr) {
            return;
        }
        cmdList->ExecuteIndirect(signature, drawCount, static_cast<D3D12Buffer *>(buffer)->GetNativeHandle(), offset, nullptr, 0);
    }

    void D3D12GraphicsEncoder::DrawIndexedIndirect(Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t stride)
    {
        if (buffer == nullptr) {
            return;
        }
        ID3D12CommandSignature *signature = device.GetIndirectSignature(IndirectKind::DRAW_INDEXED, stride);
        if (signature == nullptr) {
            return;
        }
        cmdList->ExecuteIndirect(signature, drawCount, static_cast<D3D12Buffer *>(buffer)->GetNativeHandle(), offset, nullptr, 0);
    }

    // ---- D3D12ComputeEncoder ----

    D3D12ComputeEncoder::D3D12ComputeEncoder(D3D12Device &device, ID3D12GraphicsCommandList *cmdList) : device(device), cmdList(cmdList)
    {
    }

    void D3D12ComputeEncoder::BindPipeline(ComputePipeline *pso)
    {
        auto *d3dPso         = static_cast<D3D12ComputePipeline *>(pso);
        currentRootSignature = d3dPso->GetRootSignature();
        cmdList->SetPipelineState(d3dPso->GetNativeHandle());
        if (currentRootSignature != nullptr) {
            cmdList->SetComputeRootSignature(currentRootSignature->GetNativeHandle());
        }
    }

    void D3D12ComputeEncoder::BindResourceGroup(uint32_t set, ResourceGroup *group, uint32_t numDynamicOffsets, const uint32_t *dynamicOffsets)
    {
        if (currentRootSignature == nullptr || group == nullptr || group->GetDevice() != &device) {
            return;
        }
        auto *d3dGroup = static_cast<D3D12ResourceGroup *>(group);
        if (group->GetSet() != set || !group->GetShader() ||
            static_cast<D3D12Shader *>(group->GetShader())->GetRootSignature() != currentRootSignature) {
            LOG_E(TAG, "ResourceGroup does not match the bound shader/set");
            return;
        }

        // copy staging -> current frame's shader-visible heap before binding
        d3dGroup->EnsureFrameCopy();

        auto *allocator = device.GetDescriptorAllocator();
        if (allocator == nullptr) {
            return;
        }
        ID3D12DescriptorHeap *heaps[2] = {allocator->GetCbvSrvUavHeap(), allocator->GetSamplerHeap()};
        cmdList->SetDescriptorHeaps(2, heaps);

        for (const auto &table : currentRootSignature->GetDescriptorTables(set)) {
            cmdList->SetComputeRootDescriptorTable(table.root, table.sampler ? d3dGroup->GetSamplerGpuHandle() : d3dGroup->GetCbvSrvUavGpuHandle());
        }

        // Each logical dynamic buffer may occupy distinct native roots across stages.
        const auto &dynamics = d3dGroup->GetDynamicBindings();
        for (size_t i = 0; i < dynamics.size(); ++i) {
            const auto &d = dynamics[i];
            if (d.buffer == nullptr) {
                continue;
            }
            if ((numDynamicOffsets && !dynamicOffsets) || d.buffer->GetDevice() != &device) {
                LOG_E("AuroraDX12", "Dynamic buffer offsets or owner are invalid");
                return;
            }
            const uint32_t offset = i < numDynamicOffsets ? dynamicOffsets[i] : 0;
            const auto     size   = d.buffer->GetSize();
            if (d.baseOffset >= size || offset > size - d.baseOffset || !d.range || d.range > size - d.baseOffset - offset) {
                LOG_E("AuroraDX12", "Dynamic buffer range exceeds its backing");
                return;
            }
            const auto                      effectiveOffset = d.baseOffset + offset;
            const D3D12_GPU_VIRTUAL_ADDRESS addr            = d.buffer->GetNativeHandle()->GetGPUVirtualAddress() + effectiveOffset;
            if (d.resource.type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC) {
                if (const auto error = d.buffer->CheckStorageRange(d.resource, effectiveOffset, d.range)) {
                    LOG_E("AuroraDX12", "%s", error->message.c_str());
                    return;
                }

            } else {
                if (effectiveOffset % 256 || ((d.range + 255) & ~uint64_t{255}) > size - effectiveOffset) {
                    LOG_E("AuroraDX12", "Dynamic uniform buffer alignment or backing is invalid");
                    return;
                }
            }
            for (const auto &mapping : currentRootSignature->GetDynamicBindings(set)) {
                if (mapping.binding != d.binding)
                    continue;
                switch (mapping.category) {
                case ShaderNativeCategory::CBV: cmdList->SetComputeRootConstantBufferView(mapping.root, addr); break;
                case ShaderNativeCategory::SRV: cmdList->SetComputeRootShaderResourceView(mapping.root, addr); break;
                case ShaderNativeCategory::UAV: cmdList->SetComputeRootUnorderedAccessView(mapping.root, addr); break;
                default: LOG_E(TAG, "Invalid dynamic native category"); return;
                }
            }
        }
    }

    void D3D12ComputeEncoder::BindDescriptorHeap(DescriptorHeap *heap)
    {
        auto *d3dHeap = static_cast<D3D12DescriptorHeap *>(heap);
        if (d3dHeap == nullptr) {
            return;
        }
        ID3D12DescriptorHeap *heaps[2] = {};
        uint32_t              count    = 0;
        if (d3dHeap->GetResourceHeap() != nullptr) {
            heaps[count++] = d3dHeap->GetResourceHeap();
        }
        if (d3dHeap->GetSamplerHeap() != nullptr) {
            heaps[count++] = d3dHeap->GetSamplerHeap();
        }
        if (count > 0) {
            cmdList->SetDescriptorHeaps(count, heaps);
        }
    }

    void D3D12ComputeEncoder::PushConstants(uint32_t offset, uint32_t size, const void *data)
    {
        if (currentRootSignature == nullptr) {
            return;
        }
        const uint32_t rootParam = currentRootSignature->GetPushConstantRootParam();
        if (rootParam == INVALID_INDEX) {
            return;
        }
        // D3D12 root constants are 32-bit granular; a non-aligned range cannot
        // be expressed. Reject instead of silently truncating.
        if ((offset % 4) != 0 || (size % 4) != 0) {
            LOG_E(TAG, "PushConstants requires 4-byte aligned offset/size on D3D12 (offset=%u size=%u)", offset, size);
            return;
        }
        cmdList->SetComputeRoot32BitConstants(rootParam, size / 4, data, offset / 4);
    }

    void D3D12ComputeEncoder::Dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ)
    {
        cmdList->Dispatch(groupX, groupY, groupZ);
    }

    void D3D12ComputeEncoder::DispatchIndirect(Buffer *buffer, uint64_t offset)
    {
        if (buffer == nullptr) {
            return;
        }
        ID3D12CommandSignature *signature = device.GetIndirectSignature(IndirectKind::DISPATCH, sizeof(D3D12_DISPATCH_ARGUMENTS));
        if (signature == nullptr) {
            return;
        }
        cmdList->ExecuteIndirect(signature, 1, static_cast<D3D12Buffer *>(buffer)->GetNativeHandle(), offset, nullptr, 0);
    }

    // ---- D3D12BlitEncoder ----

    D3D12BlitEncoder::D3D12BlitEncoder(D3D12Device &device, ID3D12GraphicsCommandList *cmdList) : device(device), cmdList(cmdList)
    {
    }

    void D3D12BlitEncoder::CopyBuffer(Buffer *src, Buffer *dst, uint64_t size, uint64_t srcOffset, uint64_t dstOffset)
    {
        cmdList->CopyBufferRegion(static_cast<D3D12Buffer *>(dst)->GetNativeHandle(), dstOffset, static_cast<D3D12Buffer *>(src)->GetNativeHandle(),
                                  srcOffset, size);
    }

    void D3D12BlitEncoder::CopyBufferToImage(Buffer *src, Image *dst, const std::vector<BufferImageCopy> &regions)
    {
        auto *srcBuf = static_cast<D3D12Buffer *>(src);
        auto *dstImg = static_cast<D3D12Image *>(dst);

        for (const auto &region : regions) {
            D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
            dstLoc.pResource                   = dstImg->GetNativeHandle();
            dstLoc.Type                        = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dstLoc.SubresourceIndex            = region.subRange.level + region.subRange.baseLayer * dstImg->GetMipLevels();

            D3D12_TEXTURE_COPY_LOCATION srcLoc        = {};
            srcLoc.pResource                          = srcBuf->GetNativeHandle();
            srcLoc.Type                               = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            srcLoc.PlacedFootprint.Offset             = region.bufferOffset;
            srcLoc.PlacedFootprint.Footprint.Format   = dstImg->GetDxgiFormat();
            srcLoc.PlacedFootprint.Footprint.Width    = region.imageExtent.width;
            srcLoc.PlacedFootprint.Footprint.Height   = region.imageExtent.height;
            srcLoc.PlacedFootprint.Footprint.Depth    = region.imageExtent.depth;
            const uint32_t rowLength                  = region.bufferRowLength > 0 ? region.bufferRowLength : region.imageExtent.width;
            srcLoc.PlacedFootprint.Footprint.RowPitch = static_cast<UINT>(GetImageRowPitch(dstImg->GetPixelFormat(), rowLength));

            D3D12_BOX srcBox = {};
            srcBox.left      = 0;
            srcBox.top       = 0;
            srcBox.front     = 0;
            srcBox.right     = region.imageExtent.width;
            srcBox.bottom    = region.imageExtent.height;
            srcBox.back      = region.imageExtent.depth;

            cmdList->CopyTextureRegion(&dstLoc, static_cast<UINT>(region.imageOffset.x), static_cast<UINT>(region.imageOffset.y),
                                       static_cast<UINT>(region.imageOffset.z), &srcLoc, &srcBox);
        }
    }

    void D3D12BlitEncoder::CopyImageToBuffer(Image *src, Buffer *dst, const std::vector<BufferImageCopy> &regions)
    {
        auto *srcImg = static_cast<D3D12Image *>(src);
        auto *dstBuf = static_cast<D3D12Buffer *>(dst);

        for (const auto &region : regions) {
            D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
            srcLoc.pResource                   = srcImg->GetNativeHandle();
            srcLoc.Type                        = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            srcLoc.SubresourceIndex            = region.subRange.level + region.subRange.baseLayer * srcImg->GetMipLevels();

            D3D12_TEXTURE_COPY_LOCATION dstLoc        = {};
            dstLoc.pResource                          = dstBuf->GetNativeHandle();
            dstLoc.Type                               = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            dstLoc.PlacedFootprint.Offset             = region.bufferOffset;
            dstLoc.PlacedFootprint.Footprint.Format   = srcImg->GetDxgiFormat();
            dstLoc.PlacedFootprint.Footprint.Width    = region.imageExtent.width;
            dstLoc.PlacedFootprint.Footprint.Height   = region.imageExtent.height;
            dstLoc.PlacedFootprint.Footprint.Depth    = region.imageExtent.depth;
            const uint32_t rowLength                  = region.bufferRowLength > 0 ? region.bufferRowLength : region.imageExtent.width;
            dstLoc.PlacedFootprint.Footprint.RowPitch = static_cast<UINT>(GetImageRowPitch(srcImg->GetPixelFormat(), rowLength));

            D3D12_BOX srcBox = {};
            srcBox.left      = static_cast<UINT>(region.imageOffset.x);
            srcBox.top       = static_cast<UINT>(region.imageOffset.y);
            srcBox.front     = static_cast<UINT>(region.imageOffset.z);
            srcBox.right     = static_cast<UINT>(region.imageOffset.x) + region.imageExtent.width;
            srcBox.bottom    = static_cast<UINT>(region.imageOffset.y) + region.imageExtent.height;
            srcBox.back      = static_cast<UINT>(region.imageOffset.z) + region.imageExtent.depth;

            cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, &srcBox);
        }
    }

    void D3D12BlitEncoder::BlitImage(Image *src, Image *dst, const std::vector<BlitInfo> &regions, Filter filter)
    {
        if (src == nullptr || dst == nullptr) {
            return;
        }
        auto *srcImg = static_cast<D3D12Image *>(src);
        auto *dstImg = static_cast<D3D12Image *>(dst);

        const bool sameFormat = srcImg->GetDxgiFormat() == dstImg->GetDxgiFormat();

        for (const auto &region : regions) {
            const bool sameExtent = (region.srcOffsets[1].x - region.srcOffsets[0].x) == (region.dstOffsets[1].x - region.dstOffsets[0].x) &&
                                    (region.srcOffsets[1].y - region.srcOffsets[0].y) == (region.dstOffsets[1].y - region.dstOffsets[0].y) &&
                                    (region.srcOffsets[1].z - region.srcOffsets[0].z) == (region.dstOffsets[1].z - region.dstOffsets[0].z);

            if (sameExtent && sameFormat) {
                D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
                srcLoc.pResource                   = srcImg->GetNativeHandle();
                srcLoc.Type                        = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                srcLoc.SubresourceIndex            = region.srcRange.level + region.srcRange.baseLayer * srcImg->GetMipLevels();

                D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
                dstLoc.pResource                   = dstImg->GetNativeHandle();
                dstLoc.Type                        = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                dstLoc.SubresourceIndex            = region.dstRange.level + region.dstRange.baseLayer * dstImg->GetMipLevels();

                D3D12_BOX srcBox = {};
                srcBox.left      = static_cast<UINT>(region.srcOffsets[0].x);
                srcBox.top       = static_cast<UINT>(region.srcOffsets[0].y);
                srcBox.front     = static_cast<UINT>(region.srcOffsets[0].z);
                srcBox.right     = static_cast<UINT>(region.srcOffsets[1].x);
                srcBox.bottom    = static_cast<UINT>(region.srcOffsets[1].y);
                srcBox.back      = static_cast<UINT>(region.srcOffsets[1].z);

                cmdList->CopyTextureRegion(&dstLoc, static_cast<UINT>(region.dstOffsets[0].x), static_cast<UINT>(region.dstOffsets[0].y),
                                           static_cast<UINT>(region.dstOffsets[0].z), &srcLoc, &srcBox);
            } else {
                D3D12BlitHelper *helper = device.GetBlitHelper();
                if (helper == nullptr || !helper->Blit(cmdList, src, dst, region, filter)) {
                    LOG_E(TAG, "BlitImage requires scaling/format conversion but the built-in blit pipeline is unavailable");
                }
            }
        }
    }

    void D3D12BlitEncoder::ResolveImage(Image *src, Image *dst, const std::vector<ResolveInfo> &regions)
    {
        auto *srcImg = static_cast<D3D12Image *>(src);
        auto *dstImg = static_cast<D3D12Image *>(dst);

        for (const auto &region : regions) {
            // D3D12 ResolveSubresource addresses a single subresource and cannot
            // honour regions; subresource index = mip + arrayLayer * mipLevels.
            const uint32_t srcSub = region.srcRange.level + region.srcRange.baseLayer * srcImg->GetMipLevels();
            const uint32_t dstSub = region.dstRange.level + region.dstRange.baseLayer * dstImg->GetMipLevels();
            cmdList->ResolveSubresource(dstImg->GetNativeHandle(), dstSub, srcImg->GetNativeHandle(), srcSub, dstImg->GetDxgiFormat());
        }
    }

} // namespace sky::aurora
