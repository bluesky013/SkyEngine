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

    enum class ShaderOptionType : uint8_t {
        BATCH,
        PASS,
        NUM
    };

    static constexpr uint8_t PASS_OPT_OFFSET = 0;
    static constexpr uint8_t PASS_OPT_MAX = 31;

    static constexpr uint8_t BATCH_OPT_OFFSET = PASS_OPT_MAX + 1;
    static constexpr uint8_t BATCH_OPT_MAX = 63;

    struct ShaderOptionItem {
        Name key;
        uint8_t dft  = 0;
        uint8_t bits = 1;
        ShaderOptionType type = ShaderOptionType::BATCH;
    };

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

    enum class ShaderVariantKeyInit {
        INIT,
        INVALID
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

        ShaderVariantKey(ShaderVariantKeyInit init)
        {
            u64.fill(init == ShaderVariantKeyInit::INIT ? 0 : ~(0ULL));
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

        ShaderVariantKey operator|(const ShaderVariantKey &rhs) const;
        ShaderVariantKey operator&(const ShaderVariantKey &rhs) const;

        ShaderVariantKey& operator|=(const ShaderVariantKey& rhs);
        ShaderVariantKey& operator&=(const ShaderVariantKey& rhs);

        void SetValue(uint8_t beginBit, uint8_t endBit, uint8_t val);
        uint8_t GetValue(uint8_t beginBit, uint8_t endBit) const;

        std::string ToString() const;
    };

    class ShaderVariantList {
    public:
        ShaderVariantList() = default;
        ~ShaderVariantList() = default;

        void AddEntry(const ShaderOptionEntry& entry);
        void AddOptionItem(const ShaderOptionItem &item);
        void AddDependency(const Name& key, const ShaderOptionDependency &dep);

        void SetValue(const Name &name, uint8_t val, ShaderVariantKey &keyVal)
        {
            auto iter = idMap.find(name);
            if (iter != idMap.end()) {
                const auto &range = entries[iter->second].range;
                keyVal.SetValue(range.first, range.second, val);
            }
        }

        template <typename Func>
        void ForeachOptions(Func &&fn)
        {
            for (auto &[name, id] : idMap) {
                fn(name);
            }
        }

        ShaderVariantKey GenerateDefault() const;
        std::vector<ShaderVariantKey> GeneratePermutation() const;

        void FillShaderOption(ShaderOption& option, const ShaderVariantKey& key) const;

        inline ShaderVariantKey GetPipelineMask() const { return pipelineMask; }
        inline const std::vector<ShaderOptionEntry>& GetOptionEntries() const { return entries; }
    private:
        void GeneratePermutationImpl(std::vector<ShaderVariantKey>& out, const std::vector<uint32_t> &list,
            uint32_t index, ShaderVariantKey key) const;

        std::vector<ShaderOptionEntry> entries;
        std::unordered_map<Name, std::vector<ShaderOptionDependency>> deps;
        std::unordered_map<Name, uint32_t> idMap;

        uint8_t currentBit = 0;
        ShaderVariantKey pipelineMask;
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