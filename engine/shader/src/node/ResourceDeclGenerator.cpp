//
// Created by blues on 2025/4/3.
//

#include <shader/node/ResourceDeclGenerator.h>
#include <shader/node/HLSLResourceDeclGen.h>
#include <shader/node/ResourceGroupDecl.h>

namespace sky::sl {

    std::string ResourceDeclGenerator::Generate(const ResourceGroupDecl &resGroup, ShaderLanguage language)
    {
        switch (language) {
        case ShaderLanguage::HLSL:
            return HLSLResourceDeclGenerator{}.Generate(resGroup);
        default:
            return "";
        }
    }

} // namespace sky::sl