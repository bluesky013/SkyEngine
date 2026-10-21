#pragma once

// A pass supplies one field/resource list; C++ layout is checked against compiled shader reflection.
#define AURORA_EXPAND(...) __VA_ARGS__

#ifdef __cplusplus
    #include <aurora/shader/SharedShaderTypes.h>

    #define AURORA_CPP_FIELD(owner, type, name)               alignas(FieldTraits<RgFieldType::type>::align) FieldTraits<RgFieldType::type>::Type name;
    #define AURORA_CPP_ARRAY(owner, type, name, count)        AURORA_CPP_FIELD(owner, type, name[count])
    #define AURORA_CPP_MEMBER(owner, type, name)              DescribeMember<RgFieldType::type, decltype(owner::name)>(#name, offsetof(owner, name)),
    #define AURORA_CPP_ARRAY_MEMBER(owner, type, name, count) AURORA_CPP_MEMBER(owner, type, name)
    #define AURORA_CPP_RG_FIELD(owner, type, name)            {RgFieldType::type, Name(#name), 0},
    #define AURORA_CPP_RG_ARRAY(owner, type, name, count)     {RgFieldType::type, Name(#name), count},

    #define AURORA_DECLARE_UNIFORM(params, variable, setIndex, bindingIndex, fieldList)                                                              \
        struct params {                                                                                                                              \
            fieldList(AURORA_CPP_FIELD, AURORA_CPP_ARRAY, params)                                                                                    \
        };                                                                                                                                           \
        static_assert(std::is_standard_layout_v<params>);                                                                                            \
        inline const ShaderBlockLayout &Get##params##Layout()                                                                                        \
        {                                                                                                                                            \
            static const ShaderBlockLayout layout{#variable,    #params,        setIndex,                                                            \
                                                  bindingIndex, sizeof(params), {fieldList(AURORA_CPP_MEMBER, AURORA_CPP_ARRAY_MEMBER, params)}};    \
            return layout;                                                                                                                           \
        }                                                                                                                                            \
        inline const RgBlockDesc &Get##params##BlockDesc()                                                                                           \
        {                                                                                                                                            \
            static const RgBlockDesc block{.set       = setIndex,                                                                                    \
                                           .binding   = bindingIndex,                                                                                \
                                           .blockName = Name(#params),                                                                               \
                                           .fields    = {fieldList(AURORA_CPP_RG_FIELD, AURORA_CPP_RG_ARRAY, params)}};                                 \
            return block;                                                                                                                            \
        }
    #define AURORA_DECLARE_TEXTURE(...)
    #define AURORA_DECLARE_SAMPLER(...)

    #define AURORA_SLOT_UNIFORM(params, variable, setIndex, bindingIndex, fieldList)                                                                 \
        static constexpr ShaderResourceSlot<ShaderResourceType::UNIFORM_BUFFER> variable{setIndex, bindingIndex};
    #define AURORA_SLOT_TEXTURE(shape, type, variable, setIndex, bindingIndex)                                                                       \
        static constexpr ShaderResourceSlot<AURORA_RESOURCE_TYPE_##shape> variable{setIndex, bindingIndex};
    #define AURORA_SLOT_SAMPLER(kind, variable, setIndex, bindingIndex)                                                                              \
        static constexpr ShaderResourceSlot<ShaderResourceType::SAMPLER> variable{setIndex, bindingIndex};

    #define AURORA_RESOURCE_TYPE_TEXTURE2D               ShaderResourceType::SAMPLED_IMAGE
    #define AURORA_RESOURCE_TYPE_TEXTURE2D_ARRAY         ShaderResourceType::SAMPLED_IMAGE
    #define AURORA_RESOURCE_TYPE_TEXTURECUBE             ShaderResourceType::SAMPLED_IMAGE
    #define AURORA_RESOURCE_TYPE_TEXTURECUBE_ARRAY       ShaderResourceType::SAMPLED_IMAGE
    #define AURORA_RESOURCE_TYPE_RW_TEXTURE2D            ShaderResourceType::STORAGE_IMAGE
    #define AURORA_TEXTURE_SHAPE_TEXTURE2D(type)         DescribeTexture<RgFieldType::type>(ShaderTextureDimension::D2)
    #define AURORA_TEXTURE_SHAPE_TEXTURE2D_ARRAY(type)   DescribeTexture<RgFieldType::type>(ShaderTextureDimension::D2, true)
    #define AURORA_TEXTURE_SHAPE_TEXTURECUBE(type)       DescribeTexture<RgFieldType::type>(ShaderTextureDimension::CUBE)
    #define AURORA_TEXTURE_SHAPE_TEXTURECUBE_ARRAY(type) DescribeTexture<RgFieldType::type>(ShaderTextureDimension::CUBE, true)
    #define AURORA_TEXTURE_SHAPE_RW_TEXTURE2D(type)      AURORA_TEXTURE_SHAPE_TEXTURE2D(type)
    #define AURORA_RESOURCE_UNIFORM(params, variable, setIndex, bindingIndex, fieldList)                                                             \
        {.name = #variable, .type = ShaderResourceType::UNIFORM_BUFFER, .set = setIndex, .binding = bindingIndex},
    #define AURORA_RESOURCE_TEXTURE(shape, value, variable, setIndex, bindingIndex)                                                                  \
        {.name    = #variable,                                                                                                                       \
         .type    = AURORA_RESOURCE_TYPE_##shape,                                                                                                    \
         .set     = setIndex,                                                                                                                        \
         .binding = bindingIndex,                                                                                                                    \
         .texture = AURORA_TEXTURE_SHAPE_##shape(value)},
    #define AURORA_RESOURCE_SAMPLER(kind, variable, setIndex, bindingIndex)                                                                          \
        {.name = #variable, .type = ShaderResourceType::SAMPLER, .set = setIndex, .binding = bindingIndex, .sampler = ShaderSamplerType::kind},
    #define AURORA_BLOCK_UNIFORM(params, variable, setIndex, bindingIndex, fieldList) reflection.blocks.push_back(Get##params##Layout());
    #define AURORA_BLOCK_TEXTURE(...)
    #define AURORA_BLOCK_SAMPLER(...)

    #define AURORA_DECLARE(kind, ...)  AURORA_EXPAND(AURORA_DECLARE_##kind(__VA_ARGS__))
    #define AURORA_SLOT(kind, ...)     AURORA_EXPAND(AURORA_SLOT_##kind(__VA_ARGS__))
    #define AURORA_RESOURCE(kind, ...) AURORA_EXPAND(AURORA_RESOURCE_##kind(__VA_ARGS__))
    #define AURORA_BLOCK(kind, ...)    AURORA_EXPAND(AURORA_BLOCK_##kind(__VA_ARGS__))
    #define AURORA_SHADER_PASS(pass, resourceList)                                                                                                   \
        namespace sky::aurora::shared {                                                                                                              \
            resourceList(AURORA_DECLARE) struct pass##Bindings {                                                                                     \
                resourceList(AURORA_SLOT)                                                                                                            \
            };                                                                                                                                       \
            inline const ShaderReflection &Get##pass##Reflection()                                                                                   \
            {                                                                                                                                        \
                static const ShaderReflection schema = [] {                                                                                          \
                    ShaderReflection reflection;                                                                                                     \
                    reflection.resources = {resourceList(AURORA_RESOURCE)};                                                                          \
                    resourceList(AURORA_BLOCK) return reflection;                                                                                    \
                }();                                                                                                                                 \
                return schema;                                                                                                                       \
            }                                                                                                                                        \
        }

#else

    #define AURORA_SHADER_TYPE_FLOAT                      float
    #define AURORA_SHADER_TYPE_FLOAT2                     float2
    #define AURORA_SHADER_TYPE_FLOAT3                     float3
    #define AURORA_SHADER_TYPE_FLOAT4                     float4
    #define AURORA_SHADER_TYPE_MAT4                       column_major float4x4
    #define AURORA_SHADER_TYPE_INT                        int
    #define AURORA_SHADER_TYPE_UINT                       uint
    #define AURORA_SHADER_FIELD(owner, type, name)        AURORA_SHADER_TYPE_##type name;
    #define AURORA_SHADER_ARRAY(owner, type, name, count) AURORA_SHADER_FIELD(owner, type, name[count])
    #define AURORA_DECLARE_UNIFORM(params, variable, setIndex, bindingIndex, fieldList)                                                              \
        struct params {                                                                                                                              \
            fieldList(AURORA_SHADER_FIELD, AURORA_SHADER_ARRAY, params)                                                                              \
        };                                                                                                                                           \
        [[vk::binding(bindingIndex, setIndex)]] ConstantBuffer<params> variable : register(b##bindingIndex, space##setIndex);
    #define AURORA_TEXTURE_TYPE_TEXTURE2D                    Texture2D
    #define AURORA_TEXTURE_TYPE_TEXTURE2D_ARRAY              Texture2DArray
    #define AURORA_TEXTURE_TYPE_TEXTURECUBE                  TextureCube
    #define AURORA_TEXTURE_TYPE_TEXTURECUBE_ARRAY            TextureCubeArray
    #define AURORA_TEXTURE_TYPE_RW_TEXTURE2D                 RWTexture2D
    #define AURORA_TEXTURE_REGISTER_TEXTURE2D(index)         t##index
    #define AURORA_TEXTURE_REGISTER_TEXTURE2D_ARRAY(index)   t##index
    #define AURORA_TEXTURE_REGISTER_TEXTURECUBE(index)       t##index
    #define AURORA_TEXTURE_REGISTER_TEXTURECUBE_ARRAY(index) t##index
    #define AURORA_TEXTURE_REGISTER_RW_TEXTURE2D(index)      u##index
    #define AURORA_DECLARE_TEXTURE(shape, type, variable, setIndex, bindingIndex)                                                                    \
        [[vk::binding(bindingIndex, setIndex)]] AURORA_TEXTURE_TYPE_##shape<AURORA_SHADER_TYPE_##type> variable                                      \
            : register(AURORA_TEXTURE_REGISTER_##shape(bindingIndex), space##setIndex);
    #define AURORA_SAMPLER_REGULAR    SamplerState
    #define AURORA_SAMPLER_COMPARISON SamplerComparisonState
    #define AURORA_DECLARE_SAMPLER(kind, variable, setIndex, bindingIndex)                                                                           \
        [[vk::binding(bindingIndex, setIndex)]] AURORA_SAMPLER_##kind variable : register(s##bindingIndex, space##setIndex);
    #define AURORA_DECLARE(kind, ...)              AURORA_EXPAND(AURORA_DECLARE_##kind(__VA_ARGS__))
    #define AURORA_SHADER_PASS(pass, resourceList) resourceList(AURORA_DECLARE)

#endif
