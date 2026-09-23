//
// Created on 2026/09/23.
//

#pragma once

#include <physics/PhysicsObjectId.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace sky::phy {

    // Two objects interact when each one's group is contained in the other's mask.
    struct CollisionFilter {
        uint32_t group = 0xFFFFFFFF;
        uint32_t mask  = 0xFFFFFFFF;
    };

    constexpr bool FilterAccepts(const CollisionFilter &a, const CollisionFilter &b)
    {
        return (a.group & b.mask) != 0 && (b.group & a.mask) != 0;
    }

    // Query-side filter: which layers the query looks for, plus optional handles to ignore.
    struct PhysicsQueryFilter {
        uint32_t mask = 0xFFFFFFFF;

        bool          hasIgnore = false;
        PhysicsObjectId ignore;

        std::vector<PhysicsObjectId> ignoreExtra;

        bool Ignores(PhysicsObjectId id) const
        {
            if (hasIgnore && ignore == id) {
                return true;
            }
            for (const auto &extra : ignoreExtra) {
                if (extra == id) {
                    return true;
                }
            }
            return false;
        }
    };

    // Named interaction layers map author-facing names to filter bits. Bits 0 and 1 are reserved for
    // the default and static layers so a raw CollisionFilter works without registering anything.
    class InteractionLayers {
    public:
        static constexpr uint32_t DEFAULT_BIT = 1u << 0;
        static constexpr uint32_t STATIC_BIT  = 1u << 1;

        void Reset();

        // Returns false when the name is empty or already bound to a different bit.
        bool Register(const std::string &name, uint32_t bit);

        bool Find(const std::string &name, uint32_t &bit) const;

        bool IsRegistered(const std::string &name) const;

    private:
        std::vector<std::pair<std::string, uint32_t>> layers;
    };

} // namespace sky::phy
