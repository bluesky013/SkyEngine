//
// Created on 2026/10/15.
//

#pragma once

#include <common/FrameTransientPool.h>

#include <D3D12PipelineState.h>
#include <aurora/rdg/FrozenFrameGraph.h>
#include <common/FrameContext.h>
#include <optional>
#include <variant>

namespace sky::aurora {
    class D3D12Device;
    class D3D12DeviceFrameContext;
    class D3D12CommandBuffer;
    struct D3D12Functions;
    struct D3D12GraphDraw {
        CounterPtr<D3D12GraphicsPipeline>     pipeline;
        CmdDrawLinear                         command;
        D3D12_RECT                            scissor;
        std::vector<D3D12_VERTEX_BUFFER_VIEW> vertices;
        D3D12_INDEX_BUFFER_VIEW               index{};
        CmdDrawIndexed                        indexed;
        struct DescriptorTable {
            uint32_t                    root;
            D3D12_GPU_DESCRIPTOR_HANDLE descriptor;
        };
        struct UniformAddress {
            uint32_t                  root;
            D3D12_GPU_VIRTUAL_ADDRESS address;
        };
        std::vector<DescriptorTable> tables;
        std::vector<UniformAddress>  uniforms;
    };
    struct D3D12RasterScope {
        D3D12_CPU_DESCRIPTOR_HANDLE target{};
        D3D12_VIEWPORT              viewport{};
        LoadOp                      load = LoadOp::CLEAR;
        ClearValue                  clear;
        std::vector<D3D12GraphDraw> draws;
        D3D12_CPU_DESCRIPTOR_HANDLE depth{};
        LoadOp                      depthLoad  = LoadOp::DONT_CARE;
        float                       depthClear = 1.f;
    };
    struct D3D12BufferCopyStep {
        ID3D12Resource *source       = nullptr;
        ID3D12Resource *destination  = nullptr;
        uint64_t        sourceOffset = 0, destinationOffset = 0, size = 0;
    };
    struct D3D12ImageCopyStep {
        ID3D12Resource                    *buffer = nullptr;
        ID3D12Resource                    *image  = nullptr;
        CounterPtr<Buffer>                 scratch;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        uint64_t                           bufferOffset   = 0;
        uint64_t                           rowBytes       = 0;
        uint64_t                           bufferRowPitch = 0;
        D3D12_BOX                          imageBox{};
        uint32_t                           rows        = 0;
        uint32_t                           subresource = 0;
        bool                               upload      = false;
    };
    struct D3D12ColorResolveStep {
        ID3D12Resource *source = nullptr, *destination = nullptr;
        uint32_t        sourceSubresource = 0, destinationSubresource = 0;
    };
    struct D3D12RasterFragment {
        uint32_t scope = 0, firstDraw = 0, drawCount = 0;
    };
    struct D3D12DepthResolveStep : D3D12ColorResolveStep {
        D3D12_RESOLVE_MODE mode = D3D12_RESOLVE_MODE_MIN;
    };
    struct D3D12DescriptorTable {
        uint32_t                    root;
        D3D12_GPU_DESCRIPTOR_HANDLE descriptor;
    };
    struct D3D12UniformAddress {
        uint32_t                  root;
        D3D12_GPU_VIRTUAL_ADDRESS address;
    };
    struct D3D12ComputeStep {
        CounterPtr<D3D12ComputePipeline>  pipeline;
        std::vector<D3D12DescriptorTable> tables;
        std::vector<D3D12UniformAddress>  uniforms;
        uint32_t                          groupsX = 1, groupsY = 1, groupsZ = 1;
    };
    struct D3D12CustomStep {
        std::shared_ptr<const FrameCustomPassEncoder> encoder;
        std::vector<Image *>                          images;
        std::vector<Buffer *>                         buffers;
    };
    struct D3D12EncodeTask {
        uint32_t                            logicalPass = 0;
        std::vector<D3D12_RESOURCE_BARRIER> before;
        std::variant<std::monostate,
                     D3D12BufferCopyStep,
                     D3D12ImageCopyStep,
                     D3D12RasterFragment,
                     D3D12ColorResolveStep,
                     D3D12DepthResolveStep,
                     D3D12ComputeStep,
                     D3D12CustomStep>
                                              operation;
        std::vector<D3D12_RESOURCE_BARRIER>   after;
        std::optional<D3D12_RESOURCE_BARRIER> initializeImage;
    };
    class D3D12FrameEncoder final {
    public:
        void Encode(const D3D12Functions &, D3D12CommandBuffer &, ID3D12GraphicsCommandList *, uint32_t task, ID3D12GraphicsCommandList1 *extended);
        std::optional<FrameError>     encodeError;
        bool                          requiresDepthResolve = false;
        std::vector<D3D12EncodeTask>  tasks;
        std::vector<D3D12RasterScope> scopes;
        ComPtr<ID3D12DescriptorHeap>  renderTargets;
        ComPtr<ID3D12DescriptorHeap>  depthTargets;
        ComPtr<ID3D12DescriptorHeap>  resources;
        ComPtr<ID3D12DescriptorHeap>  samplers;
    };
    class D3D12CompiledFrame final : public BackendCompiledFrame {
    public:
        CompiledFrameIdentity GetIdentity() const noexcept override
        {
            return reservation.identity;
        }
        FrameResourceBindings                   resources;
        FramePlanLease                          reservation;
        std::shared_ptr<const FrozenFrameGraph> graph;
        std::vector<uint32_t>                   livePasses;
        std::unique_ptr<D3D12FrameEncoder>      encoder;
        bool                                    coordinatorOnly = false;
    };
    using D3D12CompileResult = std::variant<FrameError, FrameNoWork, std::unique_ptr<D3D12CompiledFrame>>;
    class D3D12FrameCompiler final {
    public:
        D3D12FrameCompiler(D3D12Device &device, D3D12DeviceFrameContext &context, DeviceFrameLease lease)
            : device(device), context(context), lease(lease)
        {
        }
        D3D12CompileResult Compile(std::shared_ptr<const FrozenFrameGraph>);

    private:
        D3D12Device             &device;
        D3D12DeviceFrameContext &context;
        DeviceFrameLease         lease;
    };
} // namespace sky::aurora
