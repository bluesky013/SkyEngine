//
// Created by Zach Lee on 2023/2/19.
//

#pragma once

#include "framework/serialization/SerializationContext.h"

namespace sky {

    void *GetValue(void *ptr, uint32_t typeId, const std::string &memberName);
    const void*GetValueConst(const void *ptr, uint32_t typeId, const std::string &memberName);

    bool SetValue(void* ptr, uint32_t typeId, const std::string &memberName, const void* data);
    template <typename T, typename M>
    bool SetValue(T &ptr, const std::string &memberName, const M &data)
    {
        return SetValue(static_cast<void*>(std::addressof(ptr)), TypeInfo<T>::Hash(), memberName, static_cast<const void*>(std::addressof(data)));
    }

}