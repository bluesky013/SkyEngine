//
// Reflection validation: compare an RgBlockDesc against a platform's
// shader reflection result.
//

#pragma once

#include <aurora/shader/RgBlockDesc.h>
#include <aurora/shader/ShaderReflection.h>

#include <string>

namespace sky::aurora {

    // returns empty string when consistent; error description otherwise
    std::string ValidateBlockAgainstReflection(const RgBlockDesc &desc,
                                               const ShaderReflection &reflection);

} // namespace sky::aurora
