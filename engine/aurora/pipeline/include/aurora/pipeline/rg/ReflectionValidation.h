//
// Reflection validation: compare an RgBlockDesc against a platform's
// ShaderReflection (engine/shader) result.
//

#pragma once

#include <aurora/pipeline/rg/RgBlockDesc.h>
#include <shader/ShaderCompiler.h>

#include <string>

namespace sky::aurora {

    // returns empty string when consistent; error description otherwise
    std::string ValidateBlockAgainstReflection(const RgBlockDesc &desc,
                                               const sky::ShaderReflection &reflection);

} // namespace sky::aurora
