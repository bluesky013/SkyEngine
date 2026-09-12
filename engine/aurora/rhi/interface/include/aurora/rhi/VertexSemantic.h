//
// VertexSemantic: fixed engine-level vertex attribute semantics. The vertex
// variant reserves kVertexSemanticBits (16) bits, one per semantic, so a
// shader's vertex switches map directly onto a semantic mask.
//

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace sky::aurora {

    enum class VertexSemantic : uint8_t {
        POSITION  = 0,
        NORMAL    = 1,
        TANGENT   = 2,
        BITANGENT = 3,
        COLOR     = 4,
        UV1       = 5,
        UV2       = 6,
        UV3       = 7,
        UV4       = 8,
        CUSTOM1   = 9,
        CUSTOM2   = 10,
        CUSTOM3   = 11,
        CUSTOM4   = 12,
    };

    static constexpr uint16_t kVertexSemanticCount = 13;
    static constexpr uint16_t kVertexSemanticBits  = 16; // reserved vertex region

    inline const char *VertexSemanticName(VertexSemantic semantic)
    {
        switch (semantic) {
        case VertexSemantic::POSITION:  return "POSITION";
        case VertexSemantic::NORMAL:    return "NORMAL";
        case VertexSemantic::TANGENT:   return "TANGENT";
        case VertexSemantic::BITANGENT: return "BITANGENT";
        case VertexSemantic::COLOR:     return "COLOR";
        case VertexSemantic::UV1:       return "UV1";
        case VertexSemantic::UV2:       return "UV2";
        case VertexSemantic::UV3:       return "UV3";
        case VertexSemantic::UV4:       return "UV4";
        case VertexSemantic::CUSTOM1:   return "CUSTOM1";
        case VertexSemantic::CUSTOM2:   return "CUSTOM2";
        case VertexSemantic::CUSTOM3:   return "CUSTOM3";
        case VertexSemantic::CUSTOM4:   return "CUSTOM4";
        }
        return "UNKNOWN";
    }

    inline bool ParseVertexSemantic(std::string_view name, VertexSemantic &out)
    {
        for (uint8_t i = 0; i < kVertexSemanticCount; ++i) {
            const auto semantic = static_cast<VertexSemantic>(i);
            if (name == VertexSemanticName(semantic)) {
                out = semantic;
                return true;
            }
        }
        return false;
    }

    // fixed bitmask of which vertex semantics are present / used
    class VertexSemanticMask {
    public:
        uint16_t mask = 0;

        void Set(VertexSemantic semantic, bool on = true)
        {
            const uint16_t bit = static_cast<uint16_t>(semantic);
            if (on) {
                mask |= static_cast<uint16_t>(1u << bit);
            } else {
                mask &= static_cast<uint16_t>(~(1u << bit));
            }
        }

        bool Test(VertexSemantic semantic) const
        {
            const uint16_t bit = static_cast<uint16_t>(semantic);
            return (mask & (1u << bit)) != 0;
        }

        std::string ToString() const
        {
            std::string s;
            for (uint8_t i = 0; i < kVertexSemanticCount; ++i) {
                const auto semantic = static_cast<VertexSemantic>(i);
                if (Test(semantic)) {
                    if (!s.empty()) {
                        s += ",";
                    }
                    s += VertexSemanticName(semantic);
                }
            }
            return s;
        }
    };

} // namespace sky::aurora
