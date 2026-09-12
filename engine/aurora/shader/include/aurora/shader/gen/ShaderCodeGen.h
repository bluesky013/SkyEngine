//
// ShaderCodeGen: generate the C++ mirror of a reflected shader block.
// Consumes a ShaderBlockLayout (reflected from a .slang ParameterBlock) and
// emits a C++ header carrying the mirror struct, layout static_asserts, and
// an RgBlockDesc accessor.
//

#pragma once

#include <aurora/shader/RgBlockDesc.h>
#include <aurora/rhi/ShaderReflection.h>

#include <string>

namespace sky::aurora {

    class ShaderCodeGen {
    public:
        // Emit a C++ header (mirror struct + static_assert + GetXxxBlockDesc).
        // Returns false and fills `error` on unsupported field types.
        static bool GenerateCppHeader(const ShaderBlockLayout &block,
                                      std::string &out,
                                      std::string &error);

        // Derive an RgBlockDesc from the reflected block.
        static bool BuildBlockDesc(const ShaderBlockLayout &block,
                                   RgBlockDesc &out,
                                   std::string &error);
    };

} // namespace sky::aurora
