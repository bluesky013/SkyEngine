#pragma once

#include <array>
#include <aurora/rhi/Core.h>
#include <cstdint>
#include <span>

namespace sky::aurora {
    // Borrows declarations and preserves the first match; callers retain ownership and validate duplicates/limits.
    template <class Binding>
    class FrameBindingLookup {
    public:
        explicit FrameBindingLookup(std::span<const Binding> bindings)
        {
            for (const auto &binding : bindings) {
                if (binding.set >= sets.size() || binding.binding >= sets[binding.set].size()) {
                    continue;
                }
                auto &entry = sets[binding.set][binding.binding];
                if (entry == nullptr) {
                    entry = &binding;
                }
                usedSets |= 1u << binding.set;
            }
        }

        const Binding *Find(uint32_t set, uint32_t binding) const noexcept
        {
            return set < sets.size() && binding < sets[set].size() ? sets[set][binding] : nullptr;
        }

        bool HasSet(uint32_t set) const noexcept
        {
            return set < sets.size() && (usedSets & (1u << set)) != 0;
        }

    private:
        std::array<std::array<const Binding *, 32>, MAX_SHADER_SETS> sets{};
        uint32_t                                       usedSets = 0;
    };
} // namespace sky::aurora
