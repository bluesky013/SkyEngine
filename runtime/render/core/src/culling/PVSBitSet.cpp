//
// Created by SkyEngine on 2024/02/15.
//

#include <render/culling/PVSBitSet.h>
#include <algorithm>

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace sky {

    PVSBitSet::PVSBitSet(uint32_t numBits)
    {
        Resize(numBits);
    }

    void PVSBitSet::Resize(uint32_t numBits)
    {
        capacity = numBits;
        uint32_t numWords = (numBits + BITS_PER_WORD - 1) / BITS_PER_WORD;
        data.resize(numWords, 0);
    }

    void PVSBitSet::Set(uint32_t index)
    {
        if (index >= capacity) {
            return;
        }
        data[WordIndex(index)] |= BitMask(index);
    }

    void PVSBitSet::Clear(uint32_t index)
    {
        if (index >= capacity) {
            return;
        }
        data[WordIndex(index)] &= ~BitMask(index);
    }

    bool PVSBitSet::Test(uint32_t index) const
    {
        if (index >= capacity) {
            return false;
        }
        return (data[WordIndex(index)] & BitMask(index)) != 0;
    }

    void PVSBitSet::ClearAll()
    {
        std::fill(data.begin(), data.end(), 0ULL);
    }

    void PVSBitSet::SetAll()
    {
        std::fill(data.begin(), data.end(), ~0ULL);
        
        // Clear unused bits in the last word
        if (capacity > 0) {
            uint32_t usedBits = capacity % BITS_PER_WORD;
            if (usedBits != 0 && !data.empty()) {
                data.back() &= (1ULL << usedBits) - 1;
            }
        }
    }

    void PVSBitSet::OrWith(const PVSBitSet &other)
    {
        size_t minSize = std::min(data.size(), other.data.size());
        for (size_t i = 0; i < minSize; ++i) {
            data[i] |= other.data[i];
        }
    }

    void PVSBitSet::AndWith(const PVSBitSet &other)
    {
        size_t minSize = std::min(data.size(), other.data.size());
        for (size_t i = 0; i < minSize; ++i) {
            data[i] &= other.data[i];
        }
        // Clear remaining bits if this bitset is larger
        for (size_t i = minSize; i < data.size(); ++i) {
            data[i] = 0;
        }
    }

    uint32_t PVSBitSet::CountSet() const
    {
        uint32_t count = 0;
        for (uint64_t word : data) {
#ifdef _MSC_VER
            count += static_cast<uint32_t>(__popcnt64(word));
#else
            count += static_cast<uint32_t>(__builtin_popcountll(word));
#endif
        }
        return count;
    }

    bool PVSBitSet::Any() const
    {
        for (uint64_t word : data) {
            if (word != 0) {
                return true;
            }
        }
        return false;
    }

    void PVSBitSet::SetData(const std::vector<uint64_t>& rawData, uint32_t numBits)
    {
        capacity = numBits;
        data = rawData;
    }

} // namespace sky
