//
// Created by Zach Lee on 2022/5/29.
//

#pragma once

#include <list>
#include <vector>
#include <cstdint>

namespace sky {

    template <typename T, uint32_t N>
    class FixedChunkArray {
    public:
        static constexpr uint32_t CHUNK_NUM = N;
        using ValueType                     = T;

        FixedChunkArray() = default;
        ~FixedChunkArray()
        {
            for (auto chunk : chunks) {
                delete chunk;
            }
        }

        struct Chunk {
            T data[N];
        };

        void Emplace(const T &t)
        {
            uint32_t chunkOffset = currentNum % N;
            uint32_t chunkIdx    = currentNum / N;
            if (chunkIdx >= chunks.size()) {
                chunks.emplace_back(new Chunk());
            }

            auto chunk               = chunks[chunkIdx];
            chunk->data[chunkOffset] = t;
            currentNum++;
        }

        template <typename F>
        void ForEach(F &&f)
        {
            uint32_t index = 0;
            for (auto chunk : chunks) {
                for (uint32_t i = 0; i < N && index < currentNum; ++i, ++index) {
                    f(chunk->data[i]);
                }
            }
        }

    private:
        uint32_t             currentNum = 0;
        std::vector<Chunk *> chunks;
    };

} // namespace sky