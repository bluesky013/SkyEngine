//
// Created on 2026/04/07.
//

#include <D3D12Encoder.h>
#include <D3D12Device.h>
#include <D3D12Buffer.h>
#include <D3D12Image.h>
#include <D3D12PipelineState.h>
#include <D3D12Conversion.h>

namespace sky::aurora {

    // ---- D3D12GraphicsEncoder ----

    D3D12GraphicsEncoder::D3D12GraphicsEncoder(D3D12Device &device, ID3D12GraphicsCommandList *cmdList)
        : device(device)
        , cmdList(cmdList)
    {
    }

    void D3D12GraphicsEncoder::BeginRendering(const RenderingInfo &info)
    {
        // D3D12 uses OMSetRenderTargets; full render-target view management TBD.
        // For now set the viewport/scissor from the render area.
        D3D12_VIEWPORT vp = {};
        vp.TopLeftX = static_cast<float>(info.renderArea.offset.x);
        vp.TopLeftY = static_cast<float>(info.renderArea.offset.y);
        vp.Width    = static_cast<float>(info.renderArea.extent.width);
        vp.Height   = static_cast<float>(info.renderArea.extent.height);
        vp.MinDepth = 0.f;
        vp.MaxDepth = 1.f;
        cmdList->RSSetViewports(1, &vp);

        D3D12_RECT sc = {};
        sc.left   = info.renderArea.offset.x;
        sc.top    = info.renderArea.offset.y;
        sc.right  = info.renderArea.offset.x + static_cast<LONG>(info.renderArea.extent.width);
        sc.bottom = info.renderArea.offset.y + static_cast<LONG>(info.renderArea.extent.height);
        cmdList->RSSetScissorRects(1, &sc);
    }

    void D3D12GraphicsEncoder::EndRendering()
    {
        // No-op for D3D12 (no explicit render pass end)
    }

    void D3D12GraphicsEncoder::BindPipeline(GraphicsPipeline *pso)
    {
        auto *d3dPso = static_cast<D3D12GraphicsPipeline *>(pso);
        cmdList->SetPipelineState(d3dPso->GetNativeHandle());
    }

    void D3D12GraphicsEncoder::BindResourceGroup(uint32_t /*set*/, ResourceGroup * /*group*/)
    {
        // TODO: implement once ResourceGroup maps to D3D12 descriptor tables
    }

    void D3D12GraphicsEncoder::BindVertexBuffers(uint32_t firstBinding, uint32_t count, const BufferView *views)
    {
        constexpr uint32_t MAX_VB = 16;
        D3D12_VERTEX_BUFFER_VIEW vbViews[MAX_VB] = {};
        uint32_t n = count < MAX_VB ? count : MAX_VB;

        for (uint32_t i = 0; i < n; ++i) {
            auto *buf = static_cast<D3D12Buffer *>(views[i].buffer);
            vbViews[i].BufferLocation = buf->GetNativeHandle()->GetGPUVirtualAddress() + views[i].offset;
            vbViews[i].SizeInBytes    = static_cast<UINT>(views[i].range);
            vbViews[i].StrideInBytes  = 0; // TODO: stride from vertex layout
        }
        cmdList->IASetVertexBuffers(firstBinding, n, vbViews);
    }

    void D3D12GraphicsEncoder::BindIndexBuffer(Buffer *buffer, uint64_t offset, IndexType type)
    {
        auto *buf = static_cast<D3D12Buffer *>(buffer);
        D3D12_INDEX_BUFFER_VIEW ibView = {};
        ibView.BufferLocation = buf->GetNativeHandle()->GetGPUVirtualAddress() + offset;
        ibView.Format         = FromIndexType(type);
        ibView.SizeInBytes    = 0; // TODO: compute from buffer size
        cmdList->IASetIndexBuffer(&ibView);
    }

    void D3D12GraphicsEncoder::SetViewport(uint32_t count, const Viewport *viewports)
    {
        D3D12_VIEWPORT d3dViewports[16];
        uint32_t n = count < 16 ? count : 16;
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
        uint32_t n = count < 16 ? count : 16;
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
        // D3D12 ExecuteIndirect requires a command signature; simplified single-draw path here.
        (void)stride;
        (void)drawCount;
        // TODO: full ExecuteIndirect with command signature
    }

    void D3D12GraphicsEncoder::DrawIndexedIndirect(Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t stride)
    {
        (void)stride;
        (void)drawCount;
        // TODO: full ExecuteIndirect with command signature
    }

    // ---- D3D12ComputeEncoder ----

    D3D12ComputeEncoder::D3D12ComputeEncoder(D3D12Device &device, ID3D12GraphicsCommandList *cmdList)
        : device(device)
        , cmdList(cmdList)
    {
    }

    void D3D12ComputeEncoder::BindPipeline(ComputePipeline *pso)
    {
        auto *d3dPso = static_cast<D3D12ComputePipeline *>(pso);
        cmdList->SetPipelineState(d3dPso->GetNativeHandle());
    }

    void D3D12ComputeEncoder::BindResourceGroup(uint32_t /*set*/, ResourceGroup * /*group*/)
    {
        // TODO: implement once ResourceGroup maps to D3D12 descriptor tables
    }

    void D3D12ComputeEncoder::Dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ)
    {
        cmdList->Dispatch(groupX, groupY, groupZ);
    }

    void D3D12ComputeEncoder::DispatchIndirect(Buffer *buffer, uint64_t offset)
    {
        // TODO: ExecuteIndirect with compute command signature
    }

    // ---- D3D12BlitEncoder ----

    D3D12BlitEncoder::D3D12BlitEncoder(D3D12Device &device, ID3D12GraphicsCommandList *cmdList)
        : device(device)
        , cmdList(cmdList)
    {
    }

    void D3D12BlitEncoder::CopyBuffer(Buffer *src, Buffer *dst, uint64_t size, uint64_t srcOffset, uint64_t dstOffset)
    {
        cmdList->CopyBufferRegion(
            static_cast<D3D12Buffer *>(dst)->GetNativeHandle(), dstOffset,
            static_cast<D3D12Buffer *>(src)->GetNativeHandle(), srcOffset,
            size);
    }

    void D3D12BlitEncoder::CopyBufferToImage(Buffer *src, Image *dst, const std::vector<BufferImageCopy> &regions)
    {
        auto *srcBuf = static_cast<D3D12Buffer *>(src);
        auto *dstImg = static_cast<D3D12Image *>(dst);

        for (const auto &region : regions) {
            D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
            dstLoc.pResource        = dstImg->GetNativeHandle();
            dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dstLoc.SubresourceIndex = region.subRange.level + region.subRange.baseLayer * 1; // simplified

            D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
            srcLoc.pResource                          = srcBuf->GetNativeHandle();
            srcLoc.Type                               = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            srcLoc.PlacedFootprint.Offset             = region.bufferOffset;
            srcLoc.PlacedFootprint.Footprint.Format   = dstImg->GetDxgiFormat();
            srcLoc.PlacedFootprint.Footprint.Width    = region.imageExtent.width;
            srcLoc.PlacedFootprint.Footprint.Height   = region.imageExtent.height;
            srcLoc.PlacedFootprint.Footprint.Depth    = region.imageExtent.depth;
            srcLoc.PlacedFootprint.Footprint.RowPitch = region.bufferRowLength > 0 ? region.bufferRowLength : region.imageExtent.width * 4; // TODO: proper calculation

            D3D12_BOX srcBox = {};
            srcBox.left   = 0;
            srcBox.top    = 0;
            srcBox.front  = 0;
            srcBox.right  = region.imageExtent.width;
            srcBox.bottom = region.imageExtent.height;
            srcBox.back   = region.imageExtent.depth;

            cmdList->CopyTextureRegion(&dstLoc,
                static_cast<UINT>(region.imageOffset.x),
                static_cast<UINT>(region.imageOffset.y),
                static_cast<UINT>(region.imageOffset.z),
                &srcLoc, &srcBox);
        }
    }

    void D3D12BlitEncoder::CopyImageToBuffer(Image *src, Buffer *dst, const std::vector<BufferImageCopy> &regions)
    {
        auto *srcImg = static_cast<D3D12Image *>(src);
        auto *dstBuf = static_cast<D3D12Buffer *>(dst);

        for (const auto &region : regions) {
            D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
            srcLoc.pResource        = srcImg->GetNativeHandle();
            srcLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            srcLoc.SubresourceIndex = region.subRange.level + region.subRange.baseLayer * 1;

            D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
            dstLoc.pResource                          = dstBuf->GetNativeHandle();
            dstLoc.Type                               = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            dstLoc.PlacedFootprint.Offset             = region.bufferOffset;
            dstLoc.PlacedFootprint.Footprint.Format   = srcImg->GetDxgiFormat();
            dstLoc.PlacedFootprint.Footprint.Width    = region.imageExtent.width;
            dstLoc.PlacedFootprint.Footprint.Height   = region.imageExtent.height;
            dstLoc.PlacedFootprint.Footprint.Depth    = region.imageExtent.depth;
            dstLoc.PlacedFootprint.Footprint.RowPitch = region.bufferRowLength > 0 ? region.bufferRowLength : region.imageExtent.width * 4;

            D3D12_BOX srcBox = {};
            srcBox.left   = static_cast<UINT>(region.imageOffset.x);
            srcBox.top    = static_cast<UINT>(region.imageOffset.y);
            srcBox.front  = static_cast<UINT>(region.imageOffset.z);
            srcBox.right  = static_cast<UINT>(region.imageOffset.x) + region.imageExtent.width;
            srcBox.bottom = static_cast<UINT>(region.imageOffset.y) + region.imageExtent.height;
            srcBox.back   = static_cast<UINT>(region.imageOffset.z) + region.imageExtent.depth;

            cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, &srcBox);
        }
    }

    void D3D12BlitEncoder::BlitImage(Image * /*src*/, Image * /*dst*/, const std::vector<BlitInfo> & /*regions*/, Filter /*filter*/)
    {
        // D3D12 has no direct BlitImage equivalent; requires a full-screen pass with a shader.
        // TODO: implement via compute/graphics blit shader
    }

    void D3D12BlitEncoder::ResolveImage(Image *src, Image *dst, const std::vector<ResolveInfo> &regions)
    {
        auto *srcImg = static_cast<D3D12Image *>(src);
        auto *dstImg = static_cast<D3D12Image *>(dst);

        for (const auto &region : regions) {
            uint32_t srcSub = region.srcRange.level + region.srcRange.baseLayer * 1;
            uint32_t dstSub = region.dstRange.level + region.dstRange.baseLayer * 1;
            cmdList->ResolveSubresource(
                dstImg->GetNativeHandle(), dstSub,
                srcImg->GetNativeHandle(), srcSub,
                dstImg->GetDxgiFormat());
        }
    }

} // namespace sky::aurora
