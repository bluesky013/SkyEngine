//
// Created by Zach Lee on 2021/12/11.
//

#include <framework/serialization/AnyRT.h>
#include <framework/serialization/SerializationContext.h>

namespace sky {

    bool SetAny(Any& source, const std::string& str, const Any& any)
    {
        auto node = GetTypeMember(str, source.Info());
        if (node != nullptr && node->setterFn != nullptr) {
            return node->setterFn(source.Data(), any);
        }
        return false;
    }

    Any GetAny(Any& source, const std::string& str)
    {
        auto node = GetTypeMember(str, source.Info());
        if (node != nullptr && node->getterFn != nullptr) {
            return node->getterFn(source.Data(), false);
        }
        return Any{};
    }

}