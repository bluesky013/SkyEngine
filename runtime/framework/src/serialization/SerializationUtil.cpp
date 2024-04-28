//
// Created by Zach Lee on 2023/2/19.
//

#include <framework/serialization/SerializationUtil.h>

namespace sky {

    Any GetValue(void *ptr, const Uuid &typeId, const std::string &memberName)
    {
        auto *node = GetTypeMember(memberName, typeId);
        if (node != nullptr && node->getterFn != nullptr) {
            return node->getterFn(const_cast<void*>(ptr));
        }
        return Any{};
    }

    Any GetValueConst(const void *ptr, const Uuid &typeId, const std::string &memberName)
    {
        auto *node = GetTypeMember(memberName, typeId);
        if (node != nullptr && node->getterConstFn != nullptr) {
            return node->getterConstFn(const_cast<const void*>(ptr));
        }
        return Any{};
    }

    bool SetValueRawData(void* ptr, const Uuid &typeId, const std::string &memberName, const void* data)
    {
        auto *node = GetTypeMember(memberName, typeId);
        if (node != nullptr && node->setterFn != nullptr) {
            return node->setterFn(ptr, data);
        }
        return false;
    }

}