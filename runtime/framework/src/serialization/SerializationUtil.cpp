//
// Created by Zach Lee on 2023/2/19.
//

#include <framework/serialization/SerializationUtil.h>

namespace sky {

    void *GetValue(void *ptr, uint32_t typeId, const std::string &memberName)
    {
        auto node = GetTypeMember(memberName, typeId);
        if (node != nullptr && node->getterFn != nullptr) {
            return node->getterFn(const_cast<void*>(ptr));
        }
        return nullptr;
    }

    const void*GetValueConst(const void *ptr, uint32_t typeId, const std::string &memberName)
    {
        auto node = GetTypeMember(memberName, typeId);
        if (node != nullptr && node->getterFn != nullptr) {
            return node->getterConstFn(const_cast<const void*>(ptr));
        }
        return nullptr;
    }

    bool SetValue(void* ptr, uint32_t typeId, const std::string &memberName, const void* data)
    {
        auto node = GetTypeMember(memberName, typeId);
        if (node != nullptr && node->setterFn != nullptr) {
            return node->setterFn(ptr, data);
        }
        return false;
    }

}