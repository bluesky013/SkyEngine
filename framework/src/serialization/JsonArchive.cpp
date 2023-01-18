//
// Created by Zach Lee on 2023/1/18.
//

#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/AnyRT.h>

namespace sky {

    void JsonOutputArchive::SaveValue(const Any &any)
    {
        auto typeId = any.Info()->typeId;
        if (typeId == TypeInfo<bool>::Hash()) {
            SaveValue(*any.GetAsConst<bool>());
        } else if (typeId == TypeInfo<uint64_t>::Hash()) {
            SaveValue(*any.GetAsConst<uint64_t>());
        } else if (typeId == TypeInfo<uint32_t>::Hash()) {
            SaveValue(*any.GetAsConst<uint32_t>());
        } else if (typeId == TypeInfo<uint16_t>::Hash()) {
            SaveValue(*any.GetAsConst<uint16_t>());
        } else if (typeId == TypeInfo<uint8_t>::Hash()) {
            SaveValue(*any.GetAsConst<uint8_t>());
        } else if (typeId == TypeInfo<int64_t>::Hash()) {
            SaveValue(*any.GetAsConst<int64_t>());
        } else if (typeId == TypeInfo<int32_t>::Hash()) {
            SaveValue(*any.GetAsConst<int32_t>());
        } else if (typeId == TypeInfo<int16_t>::Hash()) {
            SaveValue(*any.GetAsConst<int16_t>());
        } else if (typeId == TypeInfo<int8_t>::Hash()) {
            SaveValue(*any.GetAsConst<int8_t>());
        } else if (typeId == TypeInfo<float>::Hash()) {
            SaveValue(*any.GetAsConst<float>());
        } else if (typeId == TypeInfo<double>::Hash()) {
            SaveValue(*any.GetAsConst<double>());
        } else if (typeId == TypeInfo<std::string>::Hash()) {
            SaveValue(*any.GetAsConst<std::string>());
        } else {
            auto node = GetTypeNode(any);
            SKY_ASSERT(node != nullptr && "type not registered");
            if (node == nullptr) {
                return;
            }

            StartObject();
            Key("classId");
            SaveValue(typeId);

            Key("elements");
            StartObject();
            for (auto &member : node->members) {
                std::string memberName = member.first.data();
                Key(memberName.c_str());
                auto memberValue = GetAny(any, memberName);
                SaveValue(memberValue);
            }
            EndObject();
            EndObject();
        }
    }

}