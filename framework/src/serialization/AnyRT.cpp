//
// Created by Zach Lee on 2021/12/11.
//

#include <framework/serialization/AnyRT.h>
#include <framework/serialization/SerializationContext.h>

namespace sky {

    bool SetAny(Any &source, const std::string &str, const Any &any)
    {
        return SetAny(const_cast<void *>(source.Data()), source.Info()->typeId, str, any);
    }

    bool SetAny(void *ptr, uint32_t id, const std::string &str, const Any &any)
    {
        auto node = GetTypeMember(str, id);
        if (node != nullptr && node->setterFn != nullptr) {
            return node->setterFn(ptr, any);
        }
        return false;
    }


    Any GetAny(const void *ptr, uint32_t id, const std::string &str)
    {
        auto node = GetTypeMember(str, id);
        if (node != nullptr && node->getterFn != nullptr) {
            return node->getterFn(const_cast<void*>(ptr), false);
        }
        return Any{};
    }

    Any GetAny(Any &source, const std::string &str)
    {
        return GetAny(source.Data(), source.Info()->typeId, str);
    }

    Any GetAny(const Any &source, const std::string &str)
    {
        return GetAny(source.Data(), source.Info()->typeId, str);
    }

} // namespace sky