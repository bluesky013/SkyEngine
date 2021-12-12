//
// Created by Zach Lee on 2021/12/9.
//

#include <framework/serialization/SerializationContext.h>
#include "JsonSerializer.h"

namespace sky {

    TypeNode* SerializationContext::FindType(const std::string& key)
    {
        auto iter = types.find(key);
        if (iter == types.end()) {
            return nullptr;
        }
        return &(iter->second);
    }

    void SerializationWriteString(const Any& any, std::string& output)
    {
        JsonSerializer serializer;
        serializer.WriteString(any, output);
    }

    void SerializationReadString(Any& any, const std::string& input)
    {
        JsonSerializer serializer;
        serializer.ReadString(any, input);
    }

}