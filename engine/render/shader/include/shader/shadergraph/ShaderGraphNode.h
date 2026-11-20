//
// Created by blues on 2026/3/10.
//

#pragma once

#include <shader/shadergraph/ShaderGraphPin.h>
#include <core/util/Uuid.h>
#include <string>
#include <vector>
#include <memory>

namespace sky {
    class JsonInputArchive;
    class JsonOutputArchive;
}

namespace sky::sg {

    // Context passed to GenerateHLSL - tracks variable counter and accumulates declarations
    struct SGCodeGenContext {
        uint32_t varCounter = 0;
        std::string declarations; // global resource declarations (textures, samplers, params)
        std::string bodyCode;     // per-pixel body code

        std::string NextVarName() { return "_sg_var" + std::to_string(varCounter++); }
    };

    // Base class for all shader graph nodes
    class SGNode {
    public:
        SGNode();
        virtual ~SGNode() = default;

        const Uuid&        GetId() const   { return id; }
        const std::string& GetName() const { return name; }
        float              GetPosX() const { return posX; }
        float              GetPosY() const { return posY; }

        void SetName(const std::string& n) { name = n; }
        void SetPosition(float x, float y) { posX = x; posY = y; }

        const std::vector<SGPin>& GetInputPins() const  { return inputPins; }
        const std::vector<SGPin>& GetOutputPins() const { return outputPins; }

        // Returns the type name used for serialization / registry
        virtual std::string GetTypeName() const = 0;

        // Returns a human-readable display name for the editor
        virtual std::string GetDisplayName() const { return name; }

        // Generate HLSL for this node.
        // inputVars: variable names (or expressions) for each input pin (parallel to inputPins).
        // outputVars: filled with the variable names produced for each output pin.
        // ctx: shared code-gen context.
        virtual void GenerateHLSL(const std::vector<std::string>& inputVars,
                                  std::vector<std::string>&       outputVars,
                                  SGCodeGenContext&                ctx) const = 0;

        virtual void LoadJson(JsonInputArchive& archive);
        virtual void SaveJson(JsonOutputArchive& archive) const;

    protected:
        Uuid        id;
        std::string name;
        float       posX = 0.0f;
        float       posY = 0.0f;

        std::vector<SGPin> inputPins;
        std::vector<SGPin> outputPins;
    };

    using SGNodePtr = std::shared_ptr<SGNode>;

    // Factory interface for creating nodes by type name
    class SGNodeFactory {
    public:
        virtual ~SGNodeFactory() = default;
        virtual SGNodePtr Create() const = 0;
    };

} // namespace sky::sg
