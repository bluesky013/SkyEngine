//
// Created by Zach Lee on 2021/12/3.
//

#pragma once

#include <core/util/Rtti.h>
#include <core/util/Uuid.h>

namespace sky {

    template <typename T>
    class Asset {
    public:
        Asset() = default;
        ~Asset() = default;

        static const char* TYPE = TypeInfo<T>::Name();
        static const uint32_t TYPE_ID = TypeInfo<T>::Hash();

    private:
        Uuid assetId;
    };

}
