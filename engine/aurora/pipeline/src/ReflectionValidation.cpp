//
// Reflection validation implementation.
//

#include <aurora/pipeline/ReflectionValidation.h>

#include <sstream>

namespace sky::aurora {

    std::string ValidateBlockAgainstReflection(const RgBlockDesc &desc,
                                               const ShaderReflection &reflection)
    {
        for (const auto &res : reflection.resources) {
            if (res.set != desc.set || res.binding != desc.binding) {
                continue;
            }

            // matched by (set, binding): check type compatibility
            const bool descIsBuffer  = desc.kind == RgBlockKind::CBUFFER || desc.kind == RgBlockKind::CBUFFER_DYNAMIC;
            const bool descIsTexture = desc.kind == RgBlockKind::RESOURCE &&
                                       !desc.fields.empty() &&
                                       (desc.fields.front().type == RgFieldType::TEXTURE2D ||
                                        desc.fields.front().type == RgFieldType::TEXTURE_CUBE);
            const bool descIsSampler = desc.kind == RgBlockKind::RESOURCE &&
                                       !desc.fields.empty() &&
                                       desc.fields.front().type == RgFieldType::SAMPLER;

            bool ok = false;
            switch (res.type) {
            case ShaderResourceType::UNIFORM_BUFFER:  ok = descIsBuffer;  break;
            case ShaderResourceType::SAMPLED_IMAGE:   ok = descIsTexture; break;
            case ShaderResourceType::SAMPLER:         ok = descIsSampler; break;
            default: ok = true; break; // storage/input attachment not checked in v1
            }

            if (!ok) {
                std::ostringstream ss;
                ss << "RgBlockDesc '" << desc.blockName.GetStr() << "' (set=" << desc.set
                   << ", binding=" << desc.binding << ") type mismatch with reflected resource '"
                   << res.name << "'";
                return ss.str();
            }
            return {}; // matched and consistent
        }

        // not found in reflection: the shader does not declare this binding
        // (dead-stripped or truly missing) -- caller decides severity
        std::ostringstream ss;
        ss << "RgBlockDesc '" << desc.blockName.GetStr() << "' (set=" << desc.set
           << ", binding=" << desc.binding << ") not present in reflection";
        return ss.str();
    }

} // namespace sky::aurora
