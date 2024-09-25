//
// Created by blues on 2024/5/5.
//

#pragma once

#include <core/type/TypeInfoObj.h>

namespace sky {

    template <typename T>
    struct TypeInfo {
        static const Uuid &RegisteredId()
        {
            return TypeInfoObj<T>::Get()->RtInfo()->registeredId;
        }
    };

} // namespace sky