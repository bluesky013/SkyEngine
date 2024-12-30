//
// Created by blues on 2024/12/30.
//

#include <shader/ShaderVariant.h>
#include <shader/ShaderCompiler.h>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/topological_sort.hpp>

namespace sky {
    using CompFunc = bool(*)(uint8_t, uint8_t );
    const CompFunc COMP_FUNCS[] = {
        [](uint8_t a, uint8_t b) -> bool{ return true; }, // ALWAYS
        [](uint8_t a, uint8_t b) -> bool{ return false; }, // NEVER
        [](uint8_t a, uint8_t b) -> bool{ return a == b; }, // EQ
        [](uint8_t a, uint8_t b) -> bool{ return a != b; }, // NE
        [](uint8_t a, uint8_t b) -> bool{ return a < b; }, // LT
        [](uint8_t a, uint8_t b) -> bool{ return a <= b; }, // LE
        [](uint8_t a, uint8_t b) -> bool{ return a > b; }, // GT
        [](uint8_t a, uint8_t b) -> bool{ return a >= b; }, // GE
    };

    void ShaderVariantKey::SetValue(uint8_t beginBit, uint8_t endBit, uint8_t val)
    {
        SKY_ASSERT(endBit - beginBit < 8);
        SKY_ASSERT(endBit < SHADER_VARIANT_KEY_LEN);

        auto S = beginBit & 0x7;
        auto E = S + (endBit - beginBit);

        auto &num = *(reinterpret_cast<uint16_t*>(&u8[beginBit >> 3]));

        auto mask = (uint16_t)((~0u << (E + 1)) | ((1u << S) - 1));
        num = num & mask | ((uint16_t)(val) << S);
    }

    uint8_t ShaderVariantKey::GetValue(uint8_t beginBit, uint8_t endBit) const
    {
        SKY_ASSERT(endBit - beginBit < 8);
        SKY_ASSERT(endBit < SHADER_VARIANT_KEY_LEN);

        auto S = beginBit & 0x7;
        auto E = S + (endBit - beginBit);

        const auto &num = *(reinterpret_cast<const uint16_t*>(&u8[beginBit >> 3]));

        auto mask = (uint16_t)((1u << (E - S + 1)) - 1);
        return static_cast<uint8_t>((num >> S) & mask);
    }

    std::string ShaderVariantKey::ToString() const
    {
        static constexpr size_t BUF_LEN = U8L * 2 + 1;
        char buf[BUF_LEN] = {};
        for (int i = 0; i < U8L; i++) {
#ifdef _MSC_VER
            sprintf_s(buf + i * 2, BUF_LEN - (i * 2), "%02x", u8[U8L - i - 1]);
#else
            sprintf(buf + i * 2, "%02x", u8[U8L - i - 1]);
#endif
        }
        buf[BUF_LEN - 1] = 0;

        return buf;
    }

    void ShaderVariantList::AddEntry(const ShaderOptionEntry& entry)
    {
        idMap[entry.key] = static_cast<uint32_t>(entries.size());
        entries.emplace_back(entry);
    }

    void ShaderVariantList::AddDependency(const Name& key, const ShaderOptionDependency &dep)
    {
        deps[key].emplace_back(dep);
    }

    using ShaderVariantGraph = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS,
        boost::property<boost::vertex_color_t, boost::default_color_type>>;

    void ShaderVariantList::GeneratePermutationImpl(std::vector<ShaderVariantKey>& out, const std::vector<uint32_t> &list,
        uint32_t index, ShaderVariantKey key) const
    {
        if (index >= list.size()) {
            out.emplace_back(key);
            return;
        }

        const auto &entry = entries[list[index]];
        auto val = static_cast<uint8_t>((1u << (entry.range.second - entry.range.first + 1)) - 1);

        auto iter = deps.find(entry.key);
        if (iter != deps.end()) {
            for (auto &dep : iter->second) {
                const auto &dstEntry = entries[idMap.at(dep.dst)];
                uint8_t dstVal = key.GetValue(dstEntry.range.first, dstEntry.range.second);
                if (!COMP_FUNCS[static_cast<uint8_t>(dep.func)](dstVal, dep.ref)) {
                    return;
                }
            }
        }

        for (uint8_t i = 0; i <= val; ++i) {
            key.SetValue(entry.range.first, entry.range.second, i);
            GeneratePermutationImpl(out, list, index + 1, key);
        }
    }

    void ShaderVariantList::FillShaderOption(ShaderOption& option, const ShaderVariantKey& key) const
    {
        for (auto &entry : entries) {
            option.SetValue(entry.key.GetStr().data(), key.GetValue(entry.range.first, entry.range.second));
        }
    }

    ShaderVariantKey ShaderVariantList::GenerateDefault() const
    {
        ShaderVariantKey defaultVal = {};

        for (auto &entry : entries) {
            defaultVal.SetValue(entry.range.first, entry.range.second, entry.dft);
        }

        return defaultVal;
    }

    std::vector<ShaderVariantKey> ShaderVariantList::GeneratePermutation() const
    {
        ShaderVariantGraph graph;
        // build graph
        uint32_t root = static_cast<uint32_t>(entries.size());

        for (const auto &[key, list] : deps) {
            for (auto &dep : list) {
                boost::add_edge(idMap.at(key), idMap.at(dep.dst), graph);
            }
        }

        for (const auto &[name, id] : idMap) {
            if (!deps.contains(name)) {
                boost::add_edge(id, root, graph);
            }
        }

        std::vector<uint32_t> container;
        boost::topological_sort(graph, std::back_inserter(container));

        // remove root entry.
        container.erase(std::remove(container.begin(), container.end(), root), container.end());

        std::vector<ShaderVariantKey> output;

        ShaderVariantKey empty = GenerateDefault();
        GeneratePermutationImpl(output, container, 0, empty);

        return output;
    }

} // namespace sky