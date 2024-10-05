//
// Created by Zach Lee on 2023/2/19.
//

#include <framework/serialization/SerializationUtil.h>

namespace sky {

    Any GetValueRaw(void *ptr, const Uuid &typeId, const std::string_view &memberName)
    {
        auto *node = GetTypeMember(memberName, typeId);
        if (node != nullptr && node->getterFn != nullptr) {
            return node->getterFn(const_cast<void*>(ptr));
        }
        return Any{};
    }

    Any GetValueRawConst(const void *ptr, const Uuid &typeId, const std::string_view &memberName)
    {
        auto *node = GetTypeMember(memberName, typeId);
        if (node != nullptr && node->getterConstFn != nullptr) {
            return node->getterConstFn(const_cast<const void*>(ptr));
        }
        return Any{};
    }



    bool SetValueRaw(void* ptr, const Uuid &typeId, const std::string_view &memberName, const void* data)
    {
        auto *node = GetTypeMember(memberName, typeId);
        if (node != nullptr && node->setterFn != nullptr) {
            return node->setterFn(ptr, data);
        }
        return false;
    }

    const Any &GetTypeProperty(const Uuid &typeId, uint32_t key)
    {
        static const Any Empty;

        const auto *node = GetTypeNode(typeId);
        if (node != nullptr) {
            auto iter = node->properties.find(key);
            if (iter != node->properties.end()) {
                return iter->second;
            }
        }
        return Empty;
    }
}