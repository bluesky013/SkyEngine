//
// Built-in fullscreen blit pipeline for D3D12BlitEncoder::BlitImage when the
// source and destination extents differ (scaled / filtered). D3D12 has no
// vkCmdBlitImage equivalent, so this renders src (SRV) into dst (RTV) with an
// embedded DXIL shader. Same-extent blits use CopyTextureRegion instead.
//

#pragma once

#include <aurora/rhi/Core.h>

#include <d3d12.h>
#include <unordered_map>
#include <wrl/client.h>

namespace sky::aurora {

    using Microsoft::WRL::ComPtr;

    class D3D12Device;

    class D3D12BlitHelper {
    public:
        bool Init(D3D12Device &device);

        bool Blit(ID3D12GraphicsCommandList *cmdList, Image *src, Image *dst,
                  const BlitInfo &region, Filter filter);

    private:
        ID3D12PipelineState *GetPipeline(DXGI_FORMAT format);

        static constexpr uint32_t kSrvSlots = 256;

        D3D12Device *mDevice = nullptr;

        ComPtr<ID3D12RootSignature> mRootSignature;
        std::unordered_map<uint32_t, ComPtr<ID3D12PipelineState>> mPipelines;

        ComPtr<ID3D12DescriptorHeap> mSrvHeap;
        uint32_t mSrvIncrement = 0;
        uint32_t mSrvNext      = 0;
        uint32_t mLastFrame    = 0xFFFFFFFF;
    };

} // namespace sky::aurora
