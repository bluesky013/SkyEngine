//
// Created on 2026/10/23.
//

#include "ViewportPickIndex.h"
#include "ViewportPickMath.h"

namespace sky::editor {
    using namespace pick;
    ViewportPickIndex::ViewportPickIndex(uint64_t budget) : byteBudget(budget)
    {
    }
    uint64_t ViewportPickIndex::Bytes() const
    {
        return nodes.capacity() * sizeof(Node);
    }
    void ViewportPickIndex::Clear()
    {
        nodes.clear();
        root = free = NONE;
    }
    uint32_t ViewportPickIndex::Allocate()
    {
        if (free != NONE) {
            const auto result = free;
            free              = nodes[free].parent;
            nodes[result]     = {};
            return result;
        }
        if (nodes.size() == nodes.capacity()) {
            const auto capacity = std::min<uint64_t>(byteBudget / sizeof(Node), std::max<uint64_t>(16, nodes.capacity() * 2));
            if (capacity <= nodes.size() || capacity >= NONE)
                return NONE;
            nodes.reserve(size_t(capacity));
        }
        nodes.emplace_back();
        return uint32_t(nodes.size() - 1);
    }
    void ViewportPickIndex::Free(uint32_t node)
    {
        nodes[node].parent = free;
        free               = node;
    }
    uint32_t ViewportPickIndex::Insert(uint32_t value, const AABB &bounds)
    {
        if (!ValidBounds(bounds))
            return NONE;
        const auto leaf = Allocate();
        if (leaf == NONE)
            return NONE;
        const auto parent = root == NONE ? NONE : Allocate();
        if (root != NONE && parent == NONE) {
            Free(leaf);
            return NONE;
        }
        nodes[leaf].value  = value;
        nodes[leaf].bounds = bounds;
        Attach(leaf, parent);
        return leaf;
    }
    void ViewportPickIndex::Attach(uint32_t leaf, uint32_t parent)
    {
        if (root == NONE) {
            root = leaf;
            return;
        }
        uint32_t sibling = root;
        while (nodes[sibling].left != NONE) {
            const auto  &node = nodes[sibling];
            const auto   left = node.left, right = node.right;
            const double leftCost  = Area(Merge(nodes[left].bounds, nodes[leaf].bounds)) - Area(nodes[left].bounds);
            const double rightCost = Area(Merge(nodes[right].bounds, nodes[leaf].bounds)) - Area(nodes[right].bounds);
            sibling                = leftCost < rightCost || (leftCost == rightCost && nodes[left].height <= nodes[right].height) ? left : right;
        }
        const auto grand      = nodes[sibling].parent;
        nodes[parent].parent  = grand;
        nodes[parent].left    = sibling;
        nodes[parent].right   = leaf;
        nodes[parent].height  = nodes[sibling].height + 1;
        nodes[parent].bounds  = Merge(nodes[sibling].bounds, nodes[leaf].bounds);
        nodes[sibling].parent = nodes[leaf].parent = parent;
        if (grand == NONE)
            root = parent;
        else if (nodes[grand].left == sibling)
            nodes[grand].left = parent;
        else
            nodes[grand].right = parent;
        Refit(parent);
    }
    void ViewportPickIndex::Detach(uint32_t leaf)
    {
        if (leaf == root) {
            root = NONE;
            return;
        }
        const auto parent = nodes[leaf].parent, grand = nodes[parent].parent;
        const auto sibling = nodes[parent].left == leaf ? nodes[parent].right : nodes[parent].left;
        if (grand == NONE)
            root = sibling;
        else if (nodes[grand].left == parent)
            nodes[grand].left = sibling;
        else
            nodes[grand].right = sibling;
        nodes[sibling].parent = grand;
        nodes[leaf].parent    = NONE;
        Free(parent);
        Refit(grand);
    }
    void ViewportPickIndex::Remove(uint32_t leaf)
    {
        Detach(leaf);
        Free(leaf);
    }
    void ViewportPickIndex::Update(uint32_t leaf, const AABB &bounds)
    {
        Detach(leaf);
        nodes[leaf].bounds = bounds;
        // Detach frees the parent required for reattachment; this cannot grow storage.
        const auto parent = root == NONE ? NONE : Allocate();
        Attach(leaf, parent);
    }
    uint32_t ViewportPickIndex::Balance(uint32_t index)
    {
        auto &a = nodes[index];
        if (a.left == NONE || a.height < 2)
            return index;
        const bool rotateRight = nodes[a.left].height > nodes[a.right].height + 1;
        const bool rotateLeft  = nodes[a.right].height > nodes[a.left].height + 1;
        if (!rotateRight && !rotateLeft)
            return index;
        const auto pivot = rotateRight ? a.left : a.right;
        auto      &b     = nodes[pivot];
        const auto first = b.left, second = b.right;
        b.parent = a.parent;
        if (a.parent == NONE)
            root = pivot;
        else if (nodes[a.parent].left == index)
            nodes[a.parent].left = pivot;
        else
            nodes[a.parent].right = pivot;
        a.parent        = pivot;
        const auto high = nodes[first].height > nodes[second].height ? first : second;
        const auto low  = high == first ? second : first;
        if (rotateRight) {
            b.right = index;
            b.left  = high;
            a.left  = low;
        } else {
            b.left  = index;
            b.right = high;
            a.right = low;
        }
        nodes[high].parent = pivot;
        nodes[low].parent  = index;
        a.bounds           = Merge(nodes[a.left].bounds, nodes[a.right].bounds);
        a.height           = 1 + std::max(nodes[a.left].height, nodes[a.right].height);
        b.bounds           = Merge(nodes[b.left].bounds, nodes[b.right].bounds);
        b.height           = 1 + std::max(nodes[b.left].height, nodes[b.right].height);
        return pivot;
    }
    void ViewportPickIndex::Refit(uint32_t node)
    {
        while (node != NONE) {
            node           = Balance(node);
            auto &current  = nodes[node];
            current.bounds = Merge(nodes[current.left].bounds, nodes[current.right].bounds);
            current.height = 1 + std::max(nodes[current.left].height, nodes[current.right].height);
            node           = current.parent;
        }
    }
    bool ViewportPickIndex::Next(Vector3   origin,
                                 Vector3   direction,
                                 float     nearDistance,
                                 float     farDistance,
                                 Cursor   &cursor,
                                 uint32_t &work,
                                 uint32_t &value,
                                 float    &entry,
                                 uint64_t &visits) const
    {
        if (!cursor.started) {
            cursor.started = true;
            if (root != NONE)
                cursor.stack[cursor.size++] = root;
        }
        while (cursor.size && work) {
            --work;
            ++visits;
            const auto &node = nodes[cursor.stack[--cursor.size]];
            if (!Bounds(node.bounds, origin, direction, nearDistance, farDistance, entry))
                continue;
            if (node.left == NONE) {
                value = node.value;
                return true;
            }
            float      leftDistance = nearDistance, rightDistance = nearDistance;
            const bool left  = Bounds(nodes[node.left].bounds, origin, direction, nearDistance, farDistance, leftDistance);
            const bool right = Bounds(nodes[node.right].bounds, origin, direction, nearDistance, farDistance, rightDistance);
            if (left && right) {
                cursor.stack[cursor.size++] = leftDistance < rightDistance ? node.right : node.left;
                cursor.stack[cursor.size++] = leftDistance < rightDistance ? node.left : node.right;
            } else if (left)
                cursor.stack[cursor.size++] = node.left;
            else if (right)
                cursor.stack[cursor.size++] = node.right;
        }
        cursor.done = cursor.size == 0;
        return false;
    }
} // namespace sky::editor
