//
// Created by blues on 2024/12/30.
//

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <array>
#include <core/name/Name.h>
#include <core/platform/Platform.h>

namespace sky {
    class ShaderOption;

    struct ShaderOptionEntry {
        Name key;
        std::pair<uint8_t, uint8_t> range;
        uint8_t dft;
    };

    enum class ShaderOptionFunc : uint8_t {
        ALWAYS,
        NEVER,
        EQ,
        NE,
        LT,
        LE,
        GT,
        GE
    };

    struct ShaderOptionDependency {
        Name dst;
        ShaderOptionFunc func;
        uint8_t ref;
    };

    static constexpr uint32_t SHADER_VARIANT_KEY_LEN = 64;

    struct ShaderVariantKey {
        static constexpr uint32_t U8L = SHADER_VARIANT_KEY_LEN / 8;
        static constexpr uint32_t U16L = SHADER_VARIANT_KEY_LEN / 16;
        static constexpr uint32_t U32L = SHADER_VARIANT_KEY_LEN / 32;
        static constexpr uint32_t U64L = SHADER_VARIANT_KEY_LEN / 64;

        union {
            std::array<uint8_t , U8L>  u8;
            std::array<uint16_t, U16L> u16;
            std::array<uint32_t, U32L> u32;
            std::array<uint64_t, U64L> u64;
        };

        ShaderVariantKey()
        {
            u64.fill(0);
        }

        explicit ShaderVariantKey(uint64_t val)
        {
            u64.fill(0);
            u64[0] = val;
        }

        bool operator == (const ShaderVariantKey& rhs) const
        {
            SKY_ASSERT(U64L == 1);
            return u64[0] == rhs.u64[0];
        }

        void SetValue(uint8_t beginBit, uint8_t endBit, uint8_t val);
        uint8_t GetValue(uint8_t beginBit, uint8_t endBit) const;

        std::string ToString() const;
    };

    class ShaderVariantList {
    public:
        ShaderVariantList() = default;
        ~ShaderVariantList() = default;

        void AddEntry(const ShaderOptionEntry& entry);
        void AddDependency(const Name& key, const ShaderOptionDependency &dep);

        ShaderVariantKey GenerateDefault() const;
        std::vector<ShaderVariantKey> GeneratePermutation() const;

        void FillShaderOption(ShaderOption& option, const ShaderVariantKey& key) const;

    private:
        void GeneratePermutationImpl(std::vector<ShaderVariantKey>& out, const std::vector<uint32_t> &list,
            uint32_t index, ShaderVariantKey key) const;

        std::vector<ShaderOptionEntry> entries;
        std::unordered_map<Name, std::vector<ShaderOptionDependency>> deps;
        std::unordered_map<Name, uint32_t> idMap;
    };

} // namespace sky

#include <core/hash/Hash.h>

namespace std {

    template <>
    struct hash<sky::ShaderVariantKey> {
        size_t operator()(const sky::ShaderVariantKey &key) const noexcept
        {
            uint32_t val = 0;
            for (unsigned int i : key.u32) {
                sky::HashCombine32(val, i);
            }
            return val;
        }
    };

    template <>
    struct equal_to<sky::ShaderVariantKey> {
        bool operator()(const sky::ShaderVariantKey &x, const sky::ShaderVariantKey &y) const noexcept
        {
            return x == y;
        }
    };

} // namespace std