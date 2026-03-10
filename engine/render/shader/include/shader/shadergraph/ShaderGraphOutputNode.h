//
// Created by blues on 2026/3/10.
//

#pragma once

#include <shader/shadergraph/ShaderGraphNode.h>
#include <shader/shadergraph/ShaderGraphTypes.h>
#include <array>
#include <string>

namespace sky::sg {

    // MaterialOutputNode – the terminal node of the shader graph.
    // Each input slot corresponds to a PBR material property.
    // This node does not produce output variables; it writes to the surface output struct.
    class SGMaterialOutputNode : public SGNode {
    public:
        SGMaterialOutputNode();
        ~SGMaterialOutputNode() override = default;

        std::string GetTypeName() const override    { return "MaterialOutput"; }
        std::string GetDisplayName() const override { return "Material Output"; }

        // GenerateHLSL writes surface assignments into ctx.bodyCode
        void GenerateHLSL(const std::vector<std::string>& inputVars,
                          std::vector<std::string>& outputVars,
                          SGCodeGenContext& ctx) const override;
    };

} // namespace sky::sg
