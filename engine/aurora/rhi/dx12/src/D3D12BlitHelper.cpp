//
// Built-in fullscreen blit pipeline (scaled / filtered BlitImage).
//

#include <D3D12BlitHelper.h>

#include <D3D12Device.h>
#include <D3D12DescriptorAllocator.h>
#include <D3D12Image.h>
#include "../gen/D3D12BlitShader.h"

#include <core/logger/Logger.h>

#include <algorithm>

static const char *TAG = "AuroraDX12";

namespace sky::aurora {

    namespace {
        ImageSubRange ToSubRange(const ImageSubRangeLayers &range)
        {
            ImageSubRange out{};
            out.baseLevel = range.level;
            out.levels    = 1;
            out.baseLayer = range.baseLayer;
            out.layers    = range.layers;
            return out;
        }
    } // namespace

    bool D3D12BlitHelper::Init(D3D12Device &device)
    {
        mDevice = &device;
        auto *native = device.GetNativeHandle();

        // ---- root signature: SRV table (t0) + static linear sampler (s0) ----
        D3D12_DESCRIPTOR_RANGE range = {};
        range.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range.NumDescriptors                    = 1;
        range.BaseShaderRegister                = 0;
        range.RegisterSpace                     = 0;
        range.OffsetInDescriptorsFromTableStart = 0;

        D3D12_ROOT_PARAMETER parameter = {};
        parameter.ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameter.DescriptorTable.NumDescriptorRanges = 1;
        parameter.DescriptorTable.pDescriptorRanges   = &range;
        parameter.ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.MipLODBias       = 0.f;
        sampler.MaxAnisotropy    = 1;
        sampler.ComparisonFunc   = D3D12_COMPARISON_FUNC_ALWAYS;
        sampler.BorderColor      = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MinLOD           = 0.f;
        sampler.MaxLOD           = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister   = 0;
        sampler.RegisterSpace    = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC signatureDesc = {};
        signatureDesc.NumParameters     = 1;
        signatureDesc.pParameters       = &parameter;
        signatureDesc.NumStaticSamplers = 1;
        signatureDesc.pStaticSamplers   = &sampler;
        signatureDesc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        HRESULT hr = D3D12SerializeRootSignature(&signatureDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                                 signature.GetAddressOf(), error.GetAddressOf());
        if (FAILED(hr)) {
            if (error) {
                LOG_E(TAG, "blit root signature serialize failed: %s", static_cast<const char *>(error->GetBufferPointer()));
            }
            return false;
        }

        hr = native->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                         IID_PPV_ARGS(mRootSignature.GetAddressOf()));
        if (FAILED(hr)) {
            LOG_E(TAG, "blit root signature creation failed");
            return false;
        }

        // ---- shader-visible SRV heap (ring of transient blit sources) ----
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = kSrvSlots;
        heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        hr = native->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(mSrvHeap.GetAddressOf()));
        if (FAILED(hr)) {
            LOG_E(TAG, "blit SRV heap creation failed");
            return false;
        }
        mSrvIncrement = native->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        return true;
    }

    ID3D12PipelineState *D3D12BlitHelper::GetPipeline(DXGI_FORMAT format)
    {
        const uint32_t key = static_cast<uint32_t>(format);
        auto           iter = mPipelines.find(key);
        if (iter != mPipelines.end()) {
            return iter->second.Get();
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
        desc.pRootSignature = mRootSignature.Get();
        desc.VS             = {blit::kBlitVS, sizeof(blit::kBlitVS)};
        desc.PS             = {blit::kBlitPS, sizeof(blit::kBlitPS)};

        desc.InputLayout.pInputElementDescs = nullptr;
        desc.InputLayout.NumElements        = 0;

        auto &rs                  = desc.RasterizerState;
        rs.FillMode               = D3D12_FILL_MODE_SOLID;
        rs.CullMode               = D3D12_CULL_MODE_NONE;
        rs.FrontCounterClockwise  = FALSE;
        rs.DepthClipEnable        = TRUE;

        auto &ds          = desc.DepthStencilState;
        ds.DepthEnable    = FALSE;
        ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        ds.DepthFunc      = D3D12_COMPARISON_FUNC_ALWAYS;
        ds.StencilEnable  = FALSE;

        auto &bs                  = desc.BlendState;
        bs.AlphaToCoverageEnable  = FALSE;
        bs.IndependentBlendEnable  = FALSE;
        bs.RenderTarget[0].BlendEnable = FALSE;
        bs.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        desc.NumRenderTargets      = 1;
        desc.RTVFormats[0]         = format;
        desc.DSVFormat             = DXGI_FORMAT_UNKNOWN;
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.SampleMask            = 0xFFFFFFFFU;
        desc.SampleDesc            = {1, 0};
        desc.NodeMask              = 0;
        desc.Flags                 = D3D12_PIPELINE_STATE_FLAG_NONE;

        ComPtr<ID3D12PipelineState> pipeline;
        const HRESULT hr = mDevice->GetNativeHandle()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(pipeline.GetAddressOf()));
        if (FAILED(hr)) {
            LOG_E(TAG, "blit pipeline creation failed, hr=0x%08x", static_cast<unsigned>(hr));
            return nullptr;
        }

        auto result = mPipelines.emplace(key, std::move(pipeline));
        return result.first->second.Get();
    }

    bool D3D12BlitHelper::Blit(ID3D12GraphicsCommandList *cmdList, Image *src, Image *dst,
                               const BlitInfo &region, Filter /*filter*/)
    {
        if (mDevice == nullptr || cmdList == nullptr || src == nullptr || dst == nullptr) {
            return false;
        }

        auto *srcImage = static_cast<D3D12Image *>(src);
        auto *dstImage = static_cast<D3D12Image *>(dst);

        ID3D12PipelineState *pipeline = GetPipeline(dstImage->GetDxgiFormat());
        if (pipeline == nullptr) {
            return false;
        }

        auto *allocator = mDevice->GetDescriptorAllocator();
        if (allocator == nullptr) {
            return false;
        }

        // Rotate the SRV slot once per in-flight frame so an in-flight frame's
        // descriptor is not overwritten before the GPU consumes it.
        const uint32_t frame = allocator->GetCurrentFrame();
        if (frame != mLastFrame) {
            mSrvNext   = 0;
            mLastFrame = frame;
        }
        const uint32_t slot = mSrvNext % kSrvSlots;
        ++mSrvNext;

        D3D12_CPU_DESCRIPTOR_HANDLE srvCpu = mSrvHeap->GetCPUDescriptorHandleForHeapStart();
        srvCpu.ptr += static_cast<size_t>(slot) * mSrvIncrement;
        srcImage->CreateSRV(srvCpu);

        uint32_t rtvFirst = 0;
        if (!allocator->AllocateRtv(1, rtvFirst)) {
            LOG_E(TAG, "blit out of RTV descriptors");
            return false;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = allocator->GetRtvCpuHandle(rtvFirst);
        dstImage->CreateRTV(rtv, ToSubRange(region.dstRange));

        cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

        ID3D12DescriptorHeap *heaps[1] = {mSrvHeap.Get()};
        cmdList->SetDescriptorHeaps(1, heaps);
        cmdList->SetGraphicsRootSignature(mRootSignature.Get());
        cmdList->SetPipelineState(pipeline);

        D3D12_GPU_DESCRIPTOR_HANDLE gpu = mSrvHeap->GetGPUDescriptorHandleForHeapStart();
        gpu.ptr += static_cast<size_t>(slot) * mSrvIncrement;
        cmdList->SetGraphicsRootDescriptorTable(0, gpu);

        const int32_t x0 = std::min(region.dstOffsets[0].x, region.dstOffsets[1].x);
        const int32_t y0 = std::min(region.dstOffsets[0].y, region.dstOffsets[1].y);
        const int32_t x1 = std::max(region.dstOffsets[0].x, region.dstOffsets[1].x);
        const int32_t y1 = std::max(region.dstOffsets[0].y, region.dstOffsets[1].y);

        D3D12_VIEWPORT viewport = {};
        viewport.TopLeftX = static_cast<float>(x0);
        viewport.TopLeftY = static_cast<float>(y0);
        viewport.Width    = static_cast<float>(x1 - x0);
        viewport.Height   = static_cast<float>(y1 - y0);
        viewport.MinDepth = 0.f;
        viewport.MaxDepth = 1.f;
        cmdList->RSSetViewports(1, &viewport);

        D3D12_RECT scissor = {x0, y0, x1, y1};
        cmdList->RSSetScissorRects(1, &scissor);

        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmdList->DrawInstanced(3, 1, 0, 0);
        return true;
    }

} // namespace sky::aurora
