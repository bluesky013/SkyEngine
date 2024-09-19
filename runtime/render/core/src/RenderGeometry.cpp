//
// Created by blues on 2024/9/14.
//

#include <render/RenderGeometry.h>
#include <render/RenderSemantics.h>
#include <render/Renderer.h>
#include <render/RHI.h>
#include <core/logger/Logger.h>

static const char* TAG = "RenderGeometry";

namespace sky {

    void RenderGeometry::AddVertexAttribute(const VertexAttribute &attribute)
    {
        vertexAttributes.emplace_back(attribute);
        attributeSemantics |= attribute.sematic;
    }

    rhi::VertexAssemblyPtr RenderGeometry::Request(const RDProgramPtr& program, rhi::VertexInputPtr &vtxInput)
    {
        auto *semantics = RenderSemantics::Get();
        rhi::VertexAssembly::Descriptor assemDesc = {};
        rhi::VertexInput::Descriptor vtxDesc = {};

        std::array<uint8_t, MAX_VERTEX_BUFFER_BINDINGS> bindingHash;
        bindingHash.fill(0xFF);

        for (const auto &attr : program->GetVertexAttributes()) {
            auto semantic = semantics->QuerySemanticByName(attr.semantic);
            if (semantic == VertexSemanticFlagBit::NONE) {
                LOG_E(TAG, "Vertex Semantic not Registered %s", attr.semantic.c_str());
                return {};
            }

            auto iter  = std::find_if(vertexAttributes.begin(), vertexAttributes.end(), [semantic](const VertexAttribute &stream) -> bool {
                return stream.sematic & semantic;
            });

            if (iter == vertexAttributes.end()) {
                // geometry not compatible with shader
                return {};
            }

            const auto &stream = *iter;
            SKY_ASSERT(stream.binding < MAX_VERTEX_BUFFER_BINDINGS)

            auto &binding = bindingHash[stream.binding];
            if (binding == 0xFF) {
                rhi::VertexBindingDesc vbDesc = {};
                vbDesc.binding   = static_cast<uint32_t>(vtxDesc.bindings.size());
                vbDesc.stride    = vertexBuffers[stream.binding].stride;
                vbDesc.inputRate = stream.rate;

                binding = vbDesc.binding;
                vtxDesc.bindings.emplace_back(vbDesc);
                assemDesc.vertexBuffers.emplace_back(vertexBuffers[stream.binding].MakeView());
            }

            rhi::VertexAttributeDesc attrDesc = {};
            attrDesc.sematic  = attr.semantic;
            attrDesc.location = attr.location;
            attrDesc.binding  = binding;
            attrDesc.offset   = stream.offset;
            attrDesc.format   = stream.format;

            vtxDesc.attributes.emplace_back(attrDesc);
        }
        auto *device = RHI::Get()->GetDevice();
        vtxInput = device->CreateVertexInput(vtxDesc);
        assemDesc.vertexInput = vtxInput;
        assemDesc.indexType   = indexBuffer.indexType;
        assemDesc.indexBuffer = indexBuffer.MakeView();

        return device->CreateVertexAssembly(assemDesc);
    }

    void RenderGeometry::Reset()
    {
        vertexBuffers.clear();
        indexBuffer = IndexBuffer{};
        vaoCache.clear();
    }

    void RenderGeometry::Upload()
    {
        if (uploaded) {
            return;
        }

        auto *sm = Renderer::Get()->GetStreamingManager();
        for (auto &vb : vertexBuffers) {
            sm->UploadBuffer(vb.buffer);
        }
        if (indexBuffer.buffer) {
            sm->UploadBuffer(indexBuffer.buffer);
        }
        uploaded = true;
    }

    bool RenderGeometry::IsReady() const
    {
        for (const auto &vb : vertexBuffers) {
            if (!vb.buffer->IsReady()) {
                return false;
            }
        }
        return !indexBuffer.buffer || indexBuffer.buffer->IsReady();
    }

} // namespace sky