//
// Sparse-set storage: sparse page table + dense entity/data arrays.
// Dense arrays stay contiguous (swap-remove); iteration is pointer-jump free.
//

#pragma once

#include <core/ecs/EntityId.h>

#include <utility>
#include <vector>

namespace sky {

    template <typename T>
    class SparseSet {
    public:
        static constexpr uint32_t EMPTY = 0xFFFFFFFFu;

        SparseSet() = default;
        ~SparseSet() = default;

        SparseSet(const SparseSet &) = delete;
        SparseSet &operator=(const SparseSet &) = delete;

        bool Contains(EntityId id) const
        {
            const uint32_t index = GetEntityIndex(id);
            return index < mSparse.size() && mSparse[index] != EMPTY && mDense[mSparse[index]] == id;
        }

        T *Get(EntityId id)
        {
            return Contains(id) ? &mData[mSparse[GetEntityIndex(id)]] : nullptr;
        }

        const T *Get(EntityId id) const
        {
            return Contains(id) ? &mData[mSparse[GetEntityIndex(id)]] : nullptr;
        }

        T &Add(EntityId id, T value)
        {
            const uint32_t index = GetEntityIndex(id);
            if (index >= mSparse.size()) {
                mSparse.resize(index + 1, EMPTY);
            }
            mSparse[index] = static_cast<uint32_t>(mDense.size());
            mDense.push_back(id);
            mData.push_back(std::move(value));
            return mData.back();
        }

        void Remove(EntityId id)
        {
            if (!Contains(id)) {
                return;
            }
            const uint32_t index    = GetEntityIndex(id);
            const uint32_t denseIdx = mSparse[index];
            const uint32_t lastIdx  = static_cast<uint32_t>(mDense.size()) - 1;

            if (denseIdx != lastIdx) {
                // swap-remove: move the last entity into the freed slot
                mDense[denseIdx] = mDense[lastIdx];
                mData[denseIdx]  = std::move(mData[lastIdx]);
                mSparse[GetEntityIndex(mDense[denseIdx])] = denseIdx;
            }
            mDense.pop_back();
            mData.pop_back();
            mSparse[index] = EMPTY;
        }

        void Clear()
        {
            mSparse.clear();
            mDense.clear();
            mData.clear();
        }

        // ---- dense iteration ----
        uint32_t Size() const { return static_cast<uint32_t>(mData.size()); }
        bool Empty() const { return mData.empty(); }

        T &Data(uint32_t denseIndex) { return mData[denseIndex]; }
        const T &Data(uint32_t denseIndex) const { return mData[denseIndex]; }

        EntityId DenseEntity(uint32_t denseIndex) const { return mDense[denseIndex]; }

        const std::vector<T> &DataArray() const { return mData; }
        const std::vector<EntityId> &EntityArray() const { return mDense; }

    private:
        std::vector<uint32_t> mSparse;
        std::vector<EntityId> mDense;
        std::vector<T>        mData;
    };

} // namespace sky
