//
// Created by blues on 2024/9/14.
//

#include <render/RenderSemantics.h>

namespace sky {

    RenderSemantics::RenderSemantics()
    {
        // built in
        RegisterVertexSemantic("POSITION", VertexSemanticFlagBit::POSITION);
        RegisterVertexSemantic("UV",       VertexSemanticFlagBit::UV);
        RegisterVertexSemantic("NORMAL",   VertexSemanticFlagBit::NORMAL);
        RegisterVertexSemantic("TANGENT",  VertexSemanticFlagBit::TANGENT);
        RegisterVertexSemantic("COLOR",    VertexSemanticFlagBit::COLOR);
        RegisterVertexSemantic("JOINT",    VertexSemanticFlagBit::JOINT);
        RegisterVertexSemantic("WEIGHT",   VertexSemanticFlagBit::WEIGHT);
        RegisterVertexSemantic("INST0",    VertexSemanticFlagBit::INST0);
        RegisterVertexSemantic("INST1",    VertexSemanticFlagBit::INST1);
        RegisterVertexSemantic("INST2",    VertexSemanticFlagBit::INST2);
        RegisterVertexSemantic("INST3",    VertexSemanticFlagBit::INST3);
    }

    void RenderSemantics::RegisterVertexSemantic(const std::string &name, VertexSemanticFlagBit flag)
    {
        vtxNameMap[name] = flag;
    }

    VertexSemanticFlagBit RenderSemantics::QuerySemanticByName(const std::string &name) const
    {
        auto iter = vtxNameMap.find(name);
        return iter != vtxNameMap.end() ? iter->second : VertexSemanticFlagBit::NONE;
    }

} // namespace sky