//
// Created by Zach on 2023/1/31.
//

#pragma once

#include <memory>

namespace sky::gles {

    class CommandStorage {
    public:
        CommandStorage() = default;
        ~CommandStorage() = default;

        struct Descriptor {
            uint32_t blockSize;
        };

        void Init(const Descriptor &desc);
        uint8_t *Allocate(uint32_t size);
        void Reset();

    private:
        uint32_t total = 0;
        uint32_t offset = 0;
        std::unique_ptr<uint8_t[]> storage;
    };

}
