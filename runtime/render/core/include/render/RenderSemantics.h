//
// Created by blues on 2024/9/14.
//

#pragma once

#include <core/environment/Singleton.h>
#include <render/RenderBase.h>

namespace sky {

    class RenderSemantics : public Singleton<RenderSemantics> {
    public:
        RenderSemantics();
        ~RenderSemantics() = default;

        void RegisterVertexSemantic(const std::string &name, VertexSemanticFlagBit flag);
        VertexSemanticFlagBit QuerySemanticByName(const std::string &name) const;

    private:
        std::unordered_map<std::string, VertexSemanticFlagBit> vtxNameMap;
    };

} // namespace sky
