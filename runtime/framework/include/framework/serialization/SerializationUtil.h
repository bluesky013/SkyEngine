//
// Created by Zach Lee on 2023/2/19.
//

#pragma once

#include "framework/serialization/SerializationContext.h"

namespace sky {

    Any GetValueRaw(void *ptr, const Uuid &typeId, const std::string_view &memberName);
    Any GetValueRawConst(const void *ptr, const Uuid &typeId, const std::string_view &memberName);

    template <typename T>
    Any GetValue(T &ptr, const std::string_view &memberName)
    {
        return GetValueRaw(static_cast<void*>(std::addressof(ptr)), TypeInfo<T>::RegisteredId(), memberName);
    }

    template <typename T>
    Any GetValueConst(T &ptr, const std::string_view &memberName)
    {
        return GetValueRawConst(static_cast<void*>(std::addressof(ptr)), TypeInfo<T>::RegisteredId(), memberName);
    }

    bool SetValueRaw(void* ptr, const Uuid &typeId, const std::string_view &memberName, const void* data);

    template <typename M>
    bool SetValue(void* ptr, const Uuid &typeId, const std::string_view &memberName, const M& data)
    {
        return SetValueRaw(ptr, typeId, memberName, &data);
    }

    template <typename T, typename M>
    bool SetValue(T &ptr, const std::string_view &memberName, const M &data)
    {
        return SetValueRaw(static_cast<void*>(std::addressof(ptr)), TypeInfo<T>::RegisteredId(), memberName, static_cast<const void*>(std::addressof(data)));
    }
}