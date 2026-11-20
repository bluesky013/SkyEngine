//
// Created by blues on 2026/3/10.
//

#pragma once

#include <cstdint>
#include <string>

namespace sky::sg {

    enum class SGDataType : uint8_t {
        FLOAT,
        FLOAT2,
        FLOAT3,
        FLOAT4,
        TEXTURE2D,
        SAMPLER_STATE,
    };

    enum class SGPinDirection : uint8_t {
        INPUT,
        OUTPUT,
    };

    inline std::string SGDataTypeToHLSL(SGDataType type)
    {
        switch (type) {
            case SGDataType::FLOAT:         return "float";
            case SGDataType::FLOAT2:        return "float2";
            case SGDataType::FLOAT3:        return "float3";
            case SGDataType::FLOAT4:        return "float4";
            case SGDataType::TEXTURE2D:     return "Texture2D";
            case SGDataType::SAMPLER_STATE: return "SamplerState";
            default: return "float";
        }
    }

    inline std::string SGDataTypeToString(SGDataType type)
    {
        switch (type) {
            case SGDataType::FLOAT:         return "float";
            case SGDataType::FLOAT2:        return "float2";
            case SGDataType::FLOAT3:        return "float3";
            case SGDataType::FLOAT4:        return "float4";
            case SGDataType::TEXTURE2D:     return "texture2d";
            case SGDataType::SAMPLER_STATE: return "samplerState";
            default: return "float";
        }
    }

    inline uint8_t SGDataTypeComponents(SGDataType type)
    {
        switch (type) {
            case SGDataType::FLOAT:  return 1;
            case SGDataType::FLOAT2: return 2;
            case SGDataType::FLOAT3: return 3;
            case SGDataType::FLOAT4: return 4;
            default: return 0;
        }
    }

    // Material output slots following PBR convention
    enum class MaterialSlot : uint8_t {
        BASE_COLOR,
        METALLIC,
        ROUGHNESS,
        NORMAL,
        EMISSIVE,
        OPACITY,
        OPACITY_MASK,
        COUNT
    };

    inline std::string MaterialSlotToString(MaterialSlot slot)
    {
        switch (slot) {
            case MaterialSlot::BASE_COLOR:    return "BaseColor";
            case MaterialSlot::METALLIC:      return "Metallic";
            case MaterialSlot::ROUGHNESS:     return "Roughness";
            case MaterialSlot::NORMAL:        return "Normal";
            case MaterialSlot::EMISSIVE:      return "Emissive";
            case MaterialSlot::OPACITY:       return "Opacity";
            case MaterialSlot::OPACITY_MASK:  return "OpacityMask";
            default: return "Unknown";
        }
    }

} // namespace sky::sg
