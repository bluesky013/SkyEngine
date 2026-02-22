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

    void RenderGeometry::FillVertexBuffer(std::vector<rhi::BufferView> &vbs)
    {
        vbs.reserve(vertexBuffers.size());
        for (auto &vb : vertexBuffers) {
            vbs.emplace_back(vb.MakeView());
        }
    }

    rhi::VertexInputPtr RenderGeometry::Request(const RDProgramPtr& program) const
    {
        auto *semantics = RenderSemantics::Get();
        rhi::VertexInput::Descriptor vtxDesc = {};

        std::array<uint8_t, MAX_VERTEX_BUFFER_BINDINGS> bindingHash;
        bindingHash.fill(0xFF);

        std::vector<rhi::VertexAttributeDesc> attributes;
        std::vector<rhi::VertexBindingDesc> bindings;

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
                vbDesc.binding   = static_cast<uint32_t>(bindings.size());
                vbDesc.stride    = vertexBuffers[stream.binding].stride;
                vbDesc.inputRate = vertexBuffers[stream.binding].rate;

                binding = vbDesc.binding;
                bindings.emplace_back(vbDesc);
            }

            rhi::VertexAttributeDesc attrDesc = {};
            attrDesc.sematic  = attr.semantic.c_str();
            attrDesc.location = attr.location;
            attrDesc.binding  = binding;
            attrDesc.offset   = stream.offset;
            attrDesc.format   = stream.format;

            attributes.emplace_back(attrDesc);
        }
        auto *device = RHI::Get()->GetDevice();

        vtxDesc.attributesNum = static_cast<uint32_t>(attributes.size());
        vtxDesc.bindingsNum = static_cast<uint32_t>(bindings.size());
        vtxDesc.attributes = attributes.data();
        vtxDesc.bindings = bindings.data();

//        vtxDesc.attributes.swap(attributes);
//        vtxDesc.bindings.swap(bindings);

        return device->CreateVertexInput(vtxDesc);
    }

    rhi::VertexAssemblyPtr RenderGeometry::Request(const RDProgramPtr& program, rhi::VertexInputPtr &vtxInput) const
    {
        auto *semantics = RenderSemantics::Get();
        rhi::VertexAssembly::Descriptor assemDesc = {};
        rhi::VertexInput::Descriptor vtxDesc = {};

        std::array<uint8_t, MAX_VERTEX_BUFFER_BINDINGS> bindingHash;
        bindingHash.fill(0xFF);

        std::vector<rhi::VertexAttributeDesc> attributes;
        std::vector<rhi::VertexBindingDesc> bindings;

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
                vbDesc.binding   = static_cast<uint32_t>(bindings.size());
                vbDesc.stride    = vertexBuffers[stream.binding].stride;
                vbDesc.inputRate = vertexBuffers[stream.binding].rate;

                binding = vbDesc.binding;
                bindings.emplace_back(vbDesc);
                assemDesc.vertexBuffers.emplace_back(vertexBuffers[stream.binding].MakeView());
            }

            rhi::VertexAttributeDesc attrDesc = {};
            attrDesc.sematic  = attr.semantic.c_str();
            attrDesc.location = attr.location;
            attrDesc.binding  = binding;
            attrDesc.offset   = stream.offset;
            attrDesc.format   = stream.format;

            attributes.emplace_back(attrDesc);
        }
        auto *device = RHI::Get()->GetDevice();

        vtxDesc.attributesNum = static_cast<uint32_t>(attributes.size());
        vtxDesc.bindingsNum = static_cast<uint32_t>(bindings.size());
        vtxDesc.attributes = attributes.data();
        vtxDesc.bindings = bindings.data();

//        vtxDesc.attributes.swap(attributes);
//        vtxDesc.bindings.swap(bindings);

        vtxInput = device->CreateVertexInput(vtxDesc);
        assemDesc.vertexInput = vtxInput;
        return device->CreateVertexAssembly(assemDesc);
    }

    void RenderGeometry::Reset()
    {
        vertexBuffers.clear();
        indexBuffer = IndexBuffer{};
        uploaded = false;
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
        if (!uploaded) {
            return false;
        }

        for (const auto &vb : vertexBuffers) {
            if (!vb.buffer->IsReady()) {
                return false;
            }
        }

        return !indexBuffer.buffer || indexBuffer.buffer->IsReady();
    }

    RenderGeometryPtr RenderGeometry::Duplicate()
    {
        auto *geom               = new RenderGeometry();
        geom->vertexBuffers      = vertexBuffers;
        geom->vertexAttributes   = vertexAttributes;
        geom->indexBuffer        = indexBuffer;
        geom->dynamicVB          = dynamicVB;
        geom->attributeSemantics = attributeSemantics;
        geom->uploaded           = uploaded;
        return geom;
    }
} // namespace sky
