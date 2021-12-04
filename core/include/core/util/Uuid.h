//
// Created by Zach Lee on 2021/12/3.
//

#pragma once

#include <cstdint>
#include <string>

namespace sky {

    /**
     *
     */
    class Uuid {
    public:
        Uuid();
        ~Uuid() = default;

        static Uuid Create();

        std::string ToString() const;

    private:
        union {
            uint8_t data[16];
            uint64_t word[2];
        };
    };

}
