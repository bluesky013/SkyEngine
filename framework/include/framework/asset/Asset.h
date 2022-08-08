//
// Created by yjrj on 2022/8/8.
//

#pragma once

#include <core/util/Uuid.h>
#include <memory>

namespace sky {

    class Asset {
    public:
        Asset() = default;
        ~Asset() = default;

        void SetUuid(const Uuid& id);

        const Uuid& GetUuid() const;

    private:
        Uuid uuid;
        std::vector<uint8_t> rawData;
    };
}