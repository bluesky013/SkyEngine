//
// Created by blues on 2025/4/3.
//

#include <shader/node/ResourceDeclGenerator.h>
#include <shader/node/HLSLResourceDeclGen.h>
#include <shader/node/ResourceGroupDecl.h>

namespace sky::sl {

    std::string ResourceDeclGenerator::Generate(const ResourceGroupDecl &resGroup, ShaderLanguage language)
    {
        std::unique_ptr<Impl> impl;
        switch (language) {
        case ShaderLanguage::HLSL:
            impl = std::make_unique<HLSLResourceDeclGenerator>();
            break;
        default:
            return "";
        }
        return impl->Generate(resGroup);
    }

} // namespace sky::sl