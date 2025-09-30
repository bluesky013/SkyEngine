//
// Created by blues on 2025/10/6.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <vector>

namespace sky {

    static constexpr uint32_t MakeMagic(char c1, char c2, char c3, char c4)
    {
        return (static_cast<uint32_t>(static_cast<uint8_t>(c1)) << 24) |
               (static_cast<uint32_t>(static_cast<uint8_t>(c2)) << 16) |
               (static_cast<uint32_t>(static_cast<uint8_t>(c3)) << 8) |
               (static_cast<uint32_t>(static_cast<uint8_t>(c4)));
    }

    class BinaryData : public RefObject {
    public:
        BinaryData() = default;
        explicit BinaryData(uint32_t size);
        ~BinaryData() override;

        void Resize(uint32_t);
        uint8_t *Data() { return data.data(); }
        size_t Size() const { return data.size(); }
    private:
        std::vector<uint8_t> data;
    };
    using BinaryDataPtr = CounterPtr<BinaryData>;

} // namespace sky
