#pragma once

// A pass supplies one field/resource list; C++ layout is checked against compiled shader reflection.
#define AURORA_EXPAND(...) __VA_ARGS__

#include <aurora/rhi/ShaderParameters.h>
#include <aurora/shader/SharedShaderTypes.h>

#define AURORA_CPP_FIELD(owner, type, name)                                                                                                          \
    alignas(::sky::aurora::shared::FieldTraits<::sky::aurora::RgFieldType::type>::align)::sky::aurora::shared::FieldTraits<                          \
        ::sky::aurora::RgFieldType::type>::Type name;
#define AURORA_CPP_ARRAY(owner, type, name, count) AURORA_CPP_FIELD(owner, type, name[count])
#define AURORA_CPP_MEMBER(owner, type, name)                                                                                                         \
    ::sky::aurora::shared::DescribeMember<::sky::aurora::RgFieldType::type, decltype(owner::name)>(#name, offsetof(owner, name)),
#define AURORA_CPP_ARRAY_MEMBER(owner, type, name, count) AURORA_CPP_MEMBER(owner, type, name)
#define AURORA_CPP_RG_FIELD(owner, type, name)            {::sky::aurora::RgFieldType::type, ::sky::Name(#name), 0},
#define AURORA_CPP_RG_ARRAY(owner, type, name, count)     {::sky::aurora::RgFieldType::type, ::sky::Name(#name), count},

#define AURORA_DECLARE_UNIFORM(params, variable, setIndex, bindingIndex, fieldList)                                                                  \
    struct params {                                                                                                                                  \
        fieldList(AURORA_CPP_FIELD, AURORA_CPP_ARRAY, params) static const ::sky::aurora::ShaderCpuSchema &Schema();                                 \
    };                                                                                                                                               \
    static_assert(std::is_standard_layout_v<params>);                                                                                                \
    inline const ::sky::aurora::ShaderBlockLayout &Get##params##Layout()                                                                             \
    {                                                                                                                                                \
        static const ::sky::aurora::ShaderBlockLayout layout{                                                                                        \
            #variable, #params, setIndex, bindingIndex, sizeof(params), {fieldList(AURORA_CPP_MEMBER, AURORA_CPP_ARRAY_MEMBER, params)}};            \
        return layout;                                                                                                                               \
    }                                                                                                                                                \
    inline const ::sky::aurora::RgBlockDesc &Get##params##BlockDesc()                                                                                \
    {                                                                                                                                                \
        static const ::sky::aurora::RgBlockDesc block{.set       = setIndex,                                                                         \
                                                      .binding   = bindingIndex,                                                                     \
                                                      .blockName = ::sky::Name(#params),                                                             \
                                                      .fields    = {fieldList(AURORA_CPP_RG_FIELD, AURORA_CPP_RG_ARRAY, params)}};                      \
        return block;                                                                                                                                \
    }                                                                                                                                                \
    inline const ::sky::aurora::ShaderCpuSchema &Get##params##CpuSchema()                                                                            \
    {                                                                                                                                                \
        static const ::sky::aurora::ShaderCpuSchema schema{sizeof(params), alignof(params), Get##params##Layout().members};                          \
        return schema;                                                                                                                               \
    }                                                                                                                                                \
    inline const ::sky::aurora::ShaderCpuSchema &params::Schema()                                                                                    \
    {                                                                                                                                                \
        return Get##params##CpuSchema();                                                                                                             \
    }

#define AURORA_DECLARE_TEXTURE(...)
#define AURORA_DECLARE_SAMPLER(...)
#define AURORA_DECLARE_STRUCTURED(...)
#define AURORA_SLOT_STRUCTURED(type, variable, setIndex, bindingIndex) \
    static constexpr ::sky::aurora::ShaderResourceSlot<::sky::aurora::ShaderResourceType::STORAGE_BUFFER> variable{setIndex, bindingIndex};
#define AURORA_RESOURCE_STRUCTURED(value, variable, setIndex, bindingIndex) \
    {.name = #variable, .type = ::sky::aurora::ShaderResourceType::STORAGE_BUFFER, .set = setIndex, .binding = bindingIndex, \
     .texture = {.dimension = ::sky::aurora::ShaderTextureDimension::BUFFER}, .access = ::sky::aurora::ShaderResourceAccess::READ_ONLY, \
     .bufferKind = ::sky::aurora::ShaderBufferKind::STRUCTURED, \
     .elementStride = sizeof(::sky::aurora::shared::FieldTraits<::sky::aurora::RgFieldType::value>::Type)},
#define AURORA_BLOCK_STRUCTURED(...)

#define AURORA_SLOT_UNIFORM(params, variable, setIndex, bindingIndex, fieldList)                                                                     \
    static constexpr ::sky::aurora::ShaderUniformSlot<params> variable{{setIndex, bindingIndex}};
#define AURORA_SLOT_TEXTURE(shape, type, variable, setIndex, bindingIndex)                                                                           \
    static constexpr ::sky::aurora::ShaderResourceSlot<AURORA_RESOURCE_TYPE_##shape> variable{setIndex, bindingIndex};
#define AURORA_SLOT_SAMPLER(kind, variable, setIndex, bindingIndex)                                                                                  \
    static constexpr ::sky::aurora::ShaderResourceSlot<::sky::aurora::ShaderResourceType::SAMPLER> variable{setIndex, bindingIndex};

#define AURORA_RESOURCE_TYPE_TEXTURE2D         ::sky::aurora::ShaderResourceType::SAMPLED_IMAGE
#define AURORA_RESOURCE_TYPE_TEXTURE2D_ARRAY   ::sky::aurora::ShaderResourceType::SAMPLED_IMAGE
#define AURORA_RESOURCE_TYPE_TEXTURECUBE       ::sky::aurora::ShaderResourceType::SAMPLED_IMAGE
#define AURORA_RESOURCE_TYPE_TEXTURECUBE_ARRAY ::sky::aurora::ShaderResourceType::SAMPLED_IMAGE
#define AURORA_RESOURCE_TYPE_RW_TEXTURE2D      ::sky::aurora::ShaderResourceType::STORAGE_IMAGE
#define AURORA_TEXTURE_SHAPE_TEXTURE2D(type)                                                                                                         \
    ::sky::aurora::shared::DescribeTexture<::sky::aurora::RgFieldType::type>(::sky::aurora::ShaderTextureDimension::D2)
#define AURORA_TEXTURE_SHAPE_TEXTURE2D_ARRAY(type)                                                                                                   \
    ::sky::aurora::shared::DescribeTexture<::sky::aurora::RgFieldType::type>(::sky::aurora::ShaderTextureDimension::D2, true)
#define AURORA_TEXTURE_SHAPE_TEXTURECUBE(type)                                                                                                       \
    ::sky::aurora::shared::DescribeTexture<::sky::aurora::RgFieldType::type>(::sky::aurora::ShaderTextureDimension::CUBE)
#define AURORA_TEXTURE_SHAPE_TEXTURECUBE_ARRAY(type)                                                                                                 \
    ::sky::aurora::shared::DescribeTexture<::sky::aurora::RgFieldType::type>(::sky::aurora::ShaderTextureDimension::CUBE, true)
#define AURORA_TEXTURE_SHAPE_RW_TEXTURE2D(type) AURORA_TEXTURE_SHAPE_TEXTURE2D(type)
#define AURORA_RESOURCE_UNIFORM(params, variable, setIndex, bindingIndex, fieldList)                                                                 \
    {.name = #variable, .type = ::sky::aurora::ShaderResourceType::UNIFORM_BUFFER, .set = setIndex, .binding = bindingIndex},
#define AURORA_RESOURCE_TEXTURE(shape, value, variable, setIndex, bindingIndex)                                                                      \
    {.name    = #variable,                                                                                                                           \
     .type    = AURORA_RESOURCE_TYPE_##shape,                                                                                                        \
     .set     = setIndex,                                                                                                                            \
     .binding = bindingIndex,                                                                                                                        \
     .texture = AURORA_TEXTURE_SHAPE_##shape(value)},
#define AURORA_RESOURCE_SAMPLER(kind, variable, setIndex, bindingIndex)                                                                              \
    {.name    = #variable,                                                                                                                           \
     .type    = ::sky::aurora::ShaderResourceType::SAMPLER,                                                                                          \
     .set     = setIndex,                                                                                                                            \
     .binding = bindingIndex,                                                                                                                        \
     .sampler = ::sky::aurora::ShaderSamplerType::kind},
#define AURORA_BLOCK_UNIFORM(params, variable, setIndex, bindingIndex, fieldList) reflection.blocks.push_back(Get##params##Layout());
#define AURORA_BLOCK_TEXTURE(...)
#define AURORA_BLOCK_SAMPLER(...)

#define AURORA_DECLARE(kind, ...)  AURORA_EXPAND(AURORA_DECLARE_##kind(__VA_ARGS__))
#define AURORA_SLOT(kind, ...)     AURORA_EXPAND(AURORA_SLOT_##kind(__VA_ARGS__))
#define AURORA_RESOURCE(kind, ...) AURORA_EXPAND(AURORA_RESOURCE_##kind(__VA_ARGS__))
#define AURORA_BLOCK(kind, ...)    AURORA_EXPAND(AURORA_BLOCK_##kind(__VA_ARGS__))
#define AURORA_SHADER_PASS(ownerNamespace, pass, resourceList)                                                                                       \
    namespace ownerNamespace {                                                                                                                       \
        resourceList(AURORA_DECLARE) struct pass##Bindings {                                                                                         \
            resourceList(AURORA_SLOT)                                                                                                                \
        };                                                                                                                                           \
        inline const ::sky::aurora::ShaderReflection &Get##pass##Reflection()                                                                        \
        {                                                                                                                                            \
            static const ::sky::aurora::ShaderReflection schema = [] {                                                                               \
                ::sky::aurora::ShaderReflection reflection;                                                                                          \
                reflection.resources = {resourceList(AURORA_RESOURCE)};                                                                              \
                resourceList(AURORA_BLOCK) return reflection;                                                                                        \
            }();                                                                                                                                     \
            return schema;                                                                                                                           \
        }                                                                                                                                            \
    }
