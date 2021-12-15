//
// Created by Zach Lee on 2021/12/11.
//

#include <framework/serialization/Any.h>
#include <framework/serialization/SerializationContext.h>

namespace sky {

    bool Any::Set(const std::string& str, const Any& any)
    {
        auto node = GetTypeMember(str, info);
        if (node != nullptr && node->setterFn != nullptr) {
            return node->setterFn(Data(), any);
        }
        return false;
    }

    Any Any::Get(const std::string& str)
    {
        auto node = GetTypeMember(str, info);
        if (node != nullptr && node->getterFn != nullptr) {
            return node->getterFn(Data());
        }
        return Any{};
    }

}