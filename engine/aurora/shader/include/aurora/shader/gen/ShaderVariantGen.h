//
// ShaderVariantGen: parse the `@variant` comment block from a .slang source and
// emit the data-driven ShaderVariantSchema as a C++ header (.variant.h).
//

#pragma once

#include <aurora/shader/ShaderVariant.h>

#include <string>

namespace sky::aurora {

    class ShaderVariantGen {
    public:
        // Parse the @variant comment block, auto-assigning relative bit offsets
        // (starting at 0). `reservedBits` is read from the optional `@reserved N`
        // directive (pipeline layout files); it is 0 for per-shader files.
        // Returns false + error on malformed input, duplicate names, or >128 bits.
        static bool Parse(const std::string &source, ShaderVariantSchema &out,
                          uint16_t &reservedBits, std::string &error);

        // Parse the @vertex comment block, mapping each vertex switch to the
        // semantics it depends on:
        //   // ===== @vertex =====
        //   //   HAS_VERTEX_COLOR : COLOR
        //   //   HAS_SKIN         : CUSTOM1 CUSTOM2
        //   // ===================
        static bool ParseVertex(const std::string &source,
                                std::vector<VertexVariantDef> &out, std::string &error);

        // Emit a .variant.h header (a GetXxxVariantSchema accessor).
        static bool GenerateHeader(const std::string &schemaName,
                                   const ShaderVariantSchema &schema, std::string &out);
    };

} // namespace sky::aurora
