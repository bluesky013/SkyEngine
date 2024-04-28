//
// Created by Zach Lee on 2023/2/19.
//

#pragma once

#include "framework/serialization/SerializationContext.h"

namespace sky {

    Any GetValue(void *ptr, const Uuid &typeId, const std::string &memberName);
    Any GetValueConst(const void *ptr, const Uuid &typeId, const std::string &memberName);

    template <typename T>
    Any GetValue(T &ptr, const std::string &memberName)
    {
        return GetValue(static_cast<void*>(std::addressof(ptr)), TypeInfo<T>::RegisteredId(), memberName);
    }

    template <typename T>
    Any GetValueConst(T &ptr, const std::string &memberName)
    {
        return GetValueConst(static_cast<void*>(std::addressof(ptr)), TypeInfo<T>::RegisteredId(), memberName);
    }

    bool SetValueRawData(void* ptr, const Uuid &typeId, const std::string &memberName, const void* data);

    template <typename M>
    bool SetValue(void* ptr, const Uuid &typeId, const std::string &memberName, const M& data)
    {
        return SetValueRawData(ptr, typeId, memberName, &data);
    }

    template <typename T, typename M>
    bool SetValue(T &ptr, const std::string &memberName, const M &data)
    {
        return SetValueRawData(static_cast<void*>(std::addressof(ptr)), TypeInfo<T>::RegisteredId(), memberName, static_cast<const void*>(std::addressof(data)));
    }
}