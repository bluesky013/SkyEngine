//
// Created by Zach Lee on 2021/12/9.
//

#include <framework/serialization/SerializationContext.h>

namespace sky {

    TypeNode *SerializationContext::FindType(const std::string &key)
    {
        auto iter = lookupTable.find(key);
        return iter == lookupTable.end() ? nullptr : iter->second;
    }

    TypeNode *SerializationContext::FindTypeById(uint32_t id)
    {
        auto iter = types.find(id);
        return iter == types.end() ? nullptr : &iter->second;
    }

} // namespace sky