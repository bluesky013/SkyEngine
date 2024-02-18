//
// Created by Zach Lee on 2021/12/4.
//

#pragma once
#include <SFMT.h>
#include <mutex>

namespace sky {

    class SFMTRandom {
    public:
        explicit SFMTRandom(uint32_t s);
        SFMTRandom();
        ~SFMTRandom() = default;

        uint32_t GenU32();

        uint64_t GenU64();

    private:
        uint32_t           seed = 0;
        sfmt_t             sfmt;
        mutable std::mutex mutex;
    };

} // namespace sky