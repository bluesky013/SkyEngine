//
// Created by Zach Lee on 2023/1/18.
//

#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/AnyRT.h>

namespace sky {

    void JsonOutputArchive::SaveValueObject(const Any &any)
    {
        SaveValueObject(any.Data(), any.Info()->typeId);
    }

    void JsonOutputArchive::SaveValueObject(const void *ptr, uint32_t typeId)
    {
        if (typeId == TypeInfo<bool>::Hash()) {
            SaveValue(*static_cast<const bool*>(ptr));
        } else if (typeId == TypeInfo<uint64_t>::Hash()) {
            SaveValue(*static_cast<const uint64_t *>(ptr));
        } else if (typeId == TypeInfo<uint32_t>::Hash()) {
            SaveValue(*static_cast<const uint32_t *>(ptr));
        } else if (typeId == TypeInfo<uint16_t>::Hash()) {
            SaveValue(*static_cast<const uint16_t *>(ptr));
        } else if (typeId == TypeInfo<uint8_t>::Hash()) {
            SaveValue(*static_cast<const uint8_t *>(ptr));
        } else if (typeId == TypeInfo<int64_t>::Hash()) {
            SaveValue(*static_cast<const int64_t *>(ptr));
        } else if (typeId == TypeInfo<int32_t>::Hash()) {
            SaveValue(*static_cast<const int32_t *>(ptr));
        } else if (typeId == TypeInfo<int16_t>::Hash()) {
            SaveValue(*static_cast<const int16_t *>(ptr));
        } else if (typeId == TypeInfo<int8_t>::Hash()) {
            SaveValue(*static_cast<const int8_t *>(ptr));
        } else if (typeId == TypeInfo<float>::Hash()) {
            SaveValue(*static_cast<const float *>(ptr));
        } else if (typeId == TypeInfo<double>::Hash()) {
            SaveValue(*static_cast<const double *>(ptr));
        } else if (typeId == TypeInfo<std::string>::Hash()) {
            SaveValue(*static_cast<const std::string *>(ptr));
        } else {
            auto context = SerializationContext::Get();
            auto node = context->FindTypeById(typeId);
            SKY_ASSERT(node != nullptr && "type not registered");
            if (node == nullptr) {
                return;
            }
            if (node->serialization.jsonSave != nullptr) {
                StartObject();
                node->serialization.jsonSave(ptr, *this);
                EndObject();
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
                auto memberValue = GetAny(ptr, typeId, memberName);
                SaveValueObject(memberValue);
            }
            EndObject();
            EndObject();
        }
    }

    void JsonInputArchive::LoadValueObject(void *ptr, uint32_t typeId)
    {
        SKY_ASSERT(!stack.empty());
        auto value = stack.back();

        if (typeId == TypeInfo<bool>::Hash()) {
            *static_cast<bool *>(ptr) = static_cast<bool>(value->GetBool());
        } else if (typeId == TypeInfo<uint64_t>::Hash()) {
            *static_cast<uint64_t *>(ptr) = static_cast<uint64_t>(value->GetUint64());
        } else if (typeId == TypeInfo<uint32_t>::Hash()) {
            *static_cast<uint32_t *>(ptr) = static_cast<uint32_t>(value->GetUint());
        } else if (typeId == TypeInfo<uint16_t>::Hash()) {
            *static_cast<uint16_t *>(ptr) = static_cast<uint16_t>(value->GetUint());
        } else if (typeId == TypeInfo<uint8_t>::Hash()) {
            *static_cast<uint8_t *>(ptr) = static_cast<uint8_t>(value->GetUint());
        } else if (typeId == TypeInfo<int64_t>::Hash()) {
            *static_cast<int64_t *>(ptr) = static_cast<int64_t>(value->GetInt64());
        } else if (typeId == TypeInfo<int32_t>::Hash()) {
            *static_cast<int32_t *>(ptr) = static_cast<int32_t>(value->GetInt());
        } else if (typeId == TypeInfo<int16_t>::Hash()) {
            *static_cast<int16_t *>(ptr) = static_cast<int16_t>(value->GetInt());
        } else if (typeId == TypeInfo<int8_t>::Hash()) {
            *static_cast<int8_t *>(ptr) = static_cast<int8_t>(value->GetInt());
        } else if (typeId == TypeInfo<float>::Hash()) {
            *static_cast<float *>(ptr) = static_cast<float>(value->GetDouble());
        } else if (typeId == TypeInfo<double>::Hash()) {
            *static_cast<double *>(ptr) = static_cast<double>(value->GetDouble());
        } else if (typeId == TypeInfo<std::string>::Hash()) {
            *static_cast<std::string *>(ptr) = std::string(value->GetString());
        } else {
            SKY_ASSERT(value != nullptr && value->IsObject());

            auto node = GetTypeNode(typeId);
            SKY_ASSERT(node != nullptr && "type not registered");
            if (node == nullptr) {
                return;
            }

            if (node->serialization.jsonLoad != nullptr) {
                node->serialization.jsonLoad(ptr, *this);
                return;
            }

            SKY_ASSERT(Start("classId"));
            uint32_t id = LoadUint();
            SKY_ASSERT(id == typeId);
            End();

            SKY_ASSERT(Start("elements"));
            for (auto &member : node->members) {
                std::string memberName = member.first.data();
                SKY_ASSERT(Start(memberName));
                SetAny(ptr, typeId, memberName, LoadValueById(member.second.info->typeId));
                End();
            }

            End();
        }
    }

    Any JsonInputArchive::LoadValueById(uint32_t typeId)
    {
        auto res = MakeAny(typeId);
        LoadValueObject(res.Data(), typeId);
        return res;
    }

    bool JsonInputArchive::LoadBool()
    {
        SKY_ASSERT(!stack.empty());
        auto value = stack.back();
        SKY_ASSERT(value != nullptr && value->IsBool());
        return value->GetBool();
    }

    int32_t JsonInputArchive::LoadInt()
    {
        SKY_ASSERT(!stack.empty());
        auto value = stack.back();
        SKY_ASSERT(value != nullptr && value->IsInt());

        return value->GetInt();
    }

    uint32_t JsonInputArchive::LoadUint()
    {
        SKY_ASSERT(!stack.empty());
        auto value = stack.back();
        SKY_ASSERT(value != nullptr && value->IsUint());

        return value->GetUint();
    }

    int64_t JsonInputArchive::LoadInt64()
    {
        SKY_ASSERT(!stack.empty());
        auto value = stack.back();
        SKY_ASSERT(value != nullptr && value->IsInt64());

        return value->GetInt64();
    }

    uint64_t JsonInputArchive::LoadUint64()
    {
        SKY_ASSERT(!stack.empty());
        auto value = stack.back();
        SKY_ASSERT(value != nullptr && value->IsUint64());

        return value->GetUint64();
    }

    double JsonInputArchive::LoadDouble()
    {
        SKY_ASSERT(!stack.empty());
        auto value = stack.back();
        SKY_ASSERT(value != nullptr && value->IsDouble());

        return value->GetDouble();
    }

    std::string JsonInputArchive::LoadString()
    {
        SKY_ASSERT(!stack.empty());
        auto value = stack.back();
        SKY_ASSERT(value != nullptr && value->IsString());

        return value->GetString();
    }

}
