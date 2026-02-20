//
// Created by SkyEngine on 2024/02/15.
//

#pragma once

#include <vector>
#include <cstdint>
#include <functional>

namespace sky {

    /**
     * @brief Dynamic bitset for storing visibility information
     * 
     * A compact representation of which objects are visible from a particular cell.
     * Uses a vector of uint64_t for efficient storage and operations.
     */
    class PVSBitSet {
    public:
        static constexpr uint32_t BITS_PER_WORD = 64;

        PVSBitSet() = default;
        explicit PVSBitSet(uint32_t numBits);

        /**
         * @brief Resize the bitset to accommodate numBits
         */
        void Resize(uint32_t numBits);

        /**
         * @brief Set a specific bit to 1 (visible)
         */
        void Set(uint32_t index);

        /**
         * @brief Clear a specific bit to 0 (not visible)
         */
        void Clear(uint32_t index);

        /**
         * @brief Test if a specific bit is set
         */
        bool Test(uint32_t index) const;

        /**
         * @brief Set all bits to 0
         */
        void ClearAll();

        /**
         * @brief Set all bits to 1
         */
        void SetAll();

        /**
         * @brief Perform bitwise OR with another bitset
         */
        void OrWith(const PVSBitSet &other);

        /**
         * @brief Perform bitwise AND with another bitset
         */
        void AndWith(const PVSBitSet &other);

        /**
         * @brief Count the number of set bits
         */
        uint32_t CountSet() const;

        /**
         * @brief Get the capacity in bits
         */
        uint32_t GetCapacity() const { return capacity; }

        /**
         * @brief Check if any bit is set
         */
        bool Any() const;

        /**
         * @brief Check if no bits are set
         */
        bool None() const { return !Any(); }

        /**
         * @brief Access raw data for serialization
         */
        const std::vector<uint64_t>& GetData() const { return data; }

        /**
         * @brief Set raw data for deserialization
         */
        void SetData(const std::vector<uint64_t>& rawData, uint32_t numBits);

        /**
         * @brief Iterate over all set bits efficiently using CTZ (Count Trailing Zeros)
         * 
         * This is much faster than checking each bit individually when the bitset
         * is sparse (few bits set). Uses hardware bit-scan instructions.
         * 
         * @param callback Function called for each set bit index
         */
        template <typename Func>
        void ForEachSetBit(Func &&callback) const
        {
            for (size_t wordIdx = 0; wordIdx < data.size(); ++wordIdx) {
                uint64_t word = data[wordIdx];
                while (word != 0) {
                    // Find position of lowest set bit using CTZ
                    uint32_t bitPos = CountTrailingZeros(word);
                    uint32_t globalBitIndex = static_cast<uint32_t>(wordIdx * BITS_PER_WORD + bitPos);
                    
                    if (globalBitIndex < capacity) {
                        callback(globalBitIndex);
                    }
                    
                    // Clear the lowest set bit
                    word &= (word - 1);
                }
            }
        }

        /**
         * @brief Collect all set bit indices into a vector
         * @param result Vector to store set bit indices
         */
        void GetSetBitIndices(std::vector<uint32_t> &result) const;

        /**
         * @brief Get the first N set bit indices
         * @param result Vector to store set bit indices  
         * @param maxCount Maximum number of indices to collect
         * @return Number of indices actually collected
         */
        uint32_t GetSetBitIndices(std::vector<uint32_t> &result, uint32_t maxCount) const;

    private:
        static uint32_t WordIndex(uint32_t bitIndex) { return bitIndex / BITS_PER_WORD; }
        static uint32_t BitOffset(uint32_t bitIndex) { return bitIndex % BITS_PER_WORD; }
        static uint64_t BitMask(uint32_t bitIndex) { return 1ULL << BitOffset(bitIndex); }
        
        /**
         * @brief Count trailing zeros (position of lowest set bit)
         * 
         * PRECONDITION: value must be non-zero. Behavior is undefined for zero.
         * All callers must check value != 0 before calling this function.
         */
        static uint32_t CountTrailingZeros(uint64_t value)
        {
#ifdef _MSC_VER
            unsigned long index;
            // PRECONDITION: value != 0 (caller must ensure this)
            _BitScanForward64(&index, value);
            return static_cast<uint32_t>(index);
#else
            // PRECONDITION: value != 0 (caller must ensure this)
            return static_cast<uint32_t>(__builtin_ctzll(value));
#endif
        }

        std::vector<uint64_t> data;
        uint32_t capacity = 0;
    };

} // namespace sky
