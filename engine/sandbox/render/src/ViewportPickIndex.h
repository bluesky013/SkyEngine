//
// Created on 2026/10/23.
//

#pragma once

#include <array>
#include <core/shapes/AABB.h>
#include <cstdint>
#include <vector>

namespace sky::editor {
    class ViewportPickIndex {
    public:
        static constexpr uint32_t NONE = UINT32_MAX;
        struct Cursor {
            std::array<uint32_t, 128> stack{};
            uint32_t                  size    = 0;
            bool                      started = false, done = false;
        };
        explicit ViewportPickIndex(uint64_t budget = 32ull * 1024 * 1024);
        uint32_t Insert(uint32_t value, const AABB &bounds);
        void     Remove(uint32_t leaf);
        void     Update(uint32_t leaf, const AABB &bounds);
        bool     Next(Vector3   origin,
                      Vector3   direction,
                      float     nearDistance,
                      float     farDistance,
                      Cursor   &cursor,
                      uint32_t &work,
                      uint32_t &value,
                      float    &entry,
                      uint64_t &visits) const;
        uint64_t Bytes() const;
        void     Clear();

    private:
        struct Node {
            AABB     bounds;
            uint32_t parent = NONE, left = NONE, right = NONE, value = NONE;
            int32_t  height = 0;
        };
        uint32_t          Allocate();
        void              Free(uint32_t node);
        void              Attach(uint32_t leaf, uint32_t parent);
        void              Detach(uint32_t leaf);
        uint32_t          Balance(uint32_t node);
        void              Refit(uint32_t node);
        std::vector<Node> nodes;
        uint32_t          root = NONE, free = NONE;
        uint64_t          byteBudget;
    };
} // namespace sky::editor
