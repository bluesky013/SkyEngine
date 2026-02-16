//
// Created by SkyEngine on 2024/02/15.
//

#include <render/culling/PVSBitSet.h>
#include <render/culling/SIMDUtils.h>
#include <algorithm>

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
        if (!data.empty()) {
            simd::ZeroFill(data.data(), data.size());
        }
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
        if (minSize > 0) {
            // Use SIMD-accelerated bitwise OR
            simd::BitwiseOr(data.data(), other.data.data(), minSize);
        }
    }

    void PVSBitSet::AndWith(const PVSBitSet &other)
    {
        size_t minSize = std::min(data.size(), other.data.size());
        if (minSize > 0) {
            // Use SIMD-accelerated bitwise AND
            simd::BitwiseAnd(data.data(), other.data.data(), minSize);
        }
        // Clear remaining bits if this bitset is larger
        if (data.size() > minSize) {
            simd::ZeroFill(data.data() + minSize, data.size() - minSize);
        }
    }

    uint32_t PVSBitSet::CountSet() const
    {
        if (data.empty()) {
            return 0;
        }
        // Use SIMD-accelerated population count
        return simd::PopCountArray(data.data(), data.size());
    }

    bool PVSBitSet::Any() const
    {
        if (data.empty()) {
            return false;
        }
        // Use SIMD-accelerated any-bit-set check
        return simd::AnyBitSet(data.data(), data.size());
    }

    void PVSBitSet::SetData(const std::vector<uint64_t>& rawData, uint32_t numBits)
    {
        capacity = numBits;
        data = rawData;
    }

} // namespace sky
