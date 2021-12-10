//
// Created by Zach Lee on 2021/12/9.
//

#include <framework/serialization/SerializationContext.h>

namespace sky {

    TypeNode* SerializationContext::FindType(const std::string& key)
    {
        auto iter = types.find(key);
        if (iter == types.end()) {
            return nullptr;
        }
        return &(iter->second);
    }

}