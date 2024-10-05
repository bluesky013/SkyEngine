//
// Created by Zach Lee on 2023/1/18.
//

#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/SerializationUtil.h>

namespace sky {

    void JsonOutputArchive::SaveValueObject(const Any &any)
    {
        SaveValueObject(any.Data(), any.Info()->registeredId);
    }

    void JsonOutputArchive::SaveValueObject(const void *ptr, const Uuid &typeId)
    {
        if (typeId == TypeInfo<bool>::RegisteredId()) {
            SaveValue(*static_cast<const bool*>(ptr));
        } else if (typeId == TypeInfo<uint64_t>::RegisteredId()) {
            SaveValue(*static_cast<const uint64_t *>(ptr));
        } else if (typeId == TypeInfo<uint32_t>::RegisteredId()) {
            SaveValue(*static_cast<const uint32_t *>(ptr));
        } else if (typeId == TypeInfo<uint16_t>::RegisteredId()) {
            SaveValue(*static_cast<const uint16_t *>(ptr));
        } else if (typeId == TypeInfo<uint8_t>::RegisteredId()) {
            SaveValue(*static_cast<const uint8_t *>(ptr));
        } else if (typeId == TypeInfo<int64_t>::RegisteredId()) {
            SaveValue(*static_cast<const int64_t *>(ptr));
        } else if (typeId == TypeInfo<int32_t>::RegisteredId()) {
            SaveValue(*static_cast<const int32_t *>(ptr));
        } else if (typeId == TypeInfo<int16_t>::RegisteredId()) {
            SaveValue(*static_cast<const int16_t *>(ptr));
        } else if (typeId == TypeInfo<int8_t>::RegisteredId()) {
            SaveValue(*static_cast<const int8_t *>(ptr));
        } else if (typeId == TypeInfo<float>::RegisteredId()) {
            SaveValue(*static_cast<const float *>(ptr));
        } else if (typeId == TypeInfo<double>::RegisteredId()) {
            SaveValue(*static_cast<const double *>(ptr));
        } else if (typeId == TypeInfo<std::string>::RegisteredId()) {
            SaveValue(*static_cast<const std::string *>(ptr));
        } else {
            auto *context = SerializationContext::Get();
            auto *node = context->FindTypeById(typeId);
            SKY_ASSERT(node != nullptr && "type not registered");
            if (node == nullptr) {
                return;
            }
            if (node->serialization.jsonSave != nullptr) {
                node->serialization.jsonSave(ptr, *this);
                return;
            }

            if (node->info->staticInfo->isEnum) {
                SaveValueObject(ptr, node->info->underlyingTypeId);
            } else {
                StartObject();
                Key("classId");
                SaveValue(typeId.ToString());

                Key("elements");
                StartObject();
                for (auto &member: node->members) {
                    std::string memberName = member.first.data();
                    Key(memberName.c_str());
                    Any value = GetValueRawConst(ptr, typeId, memberName);

                    if (member.second.info->containerInfo != nullptr) {
                        auto *containerInfo = member.second.info->containerInfo;
                        if (containerInfo->sequenceView != nullptr) {
                            SequenceVisitor visitor(containerInfo, value.Data());
                            StartArray();

                            auto count = visitor.Count();
                            for (size_t i = 0; i < count; ++i) {
                                SaveValueObject(visitor.GetByIndex(i), visitor.GetValueType());
                            }

                            EndArray();
                        }
                    } else {
                        SaveValueObject(value.Data(), member.second.info->registeredId);
                    }
                }
                EndObject();
                EndObject();
            }
        }
    }

    void JsonInputArchive::LoadValueById(void *ptr, const Uuid &typeId)
    {
        SKY_ASSERT(!stack.empty());
        const auto *value = stack.back();

        if (typeId == TypeInfo<bool>::RegisteredId()) {
            *static_cast<bool *>(ptr) = static_cast<bool>(value->GetBool());
        } else if (typeId == TypeInfo<uint64_t>::RegisteredId()) {
            *static_cast<uint64_t *>(ptr) = static_cast<uint64_t>(value->GetUint64());
        } else if (typeId == TypeInfo<uint32_t>::RegisteredId()) {
            *static_cast<uint32_t *>(ptr) = static_cast<uint32_t>(value->GetUint());
        } else if (typeId == TypeInfo<uint16_t>::RegisteredId()) {
            *static_cast<uint16_t *>(ptr) = static_cast<uint16_t>(value->GetUint());
        } else if (typeId == TypeInfo<uint8_t>::RegisteredId()) {
            *static_cast<uint8_t *>(ptr) = static_cast<uint8_t>(value->GetUint());
        } else if (typeId == TypeInfo<int64_t>::RegisteredId()) {
            *static_cast<int64_t *>(ptr) = static_cast<int64_t>(value->GetInt64());
        } else if (typeId == TypeInfo<int32_t>::RegisteredId()) {
            *static_cast<int32_t *>(ptr) = static_cast<int32_t>(value->GetInt());
        } else if (typeId == TypeInfo<int16_t>::RegisteredId()) {
            *static_cast<int16_t *>(ptr) = static_cast<int16_t>(value->GetInt());
        } else if (typeId == TypeInfo<int8_t>::RegisteredId()) {
            *static_cast<int8_t *>(ptr) = static_cast<int8_t>(value->GetInt());
        } else if (typeId == TypeInfo<float>::RegisteredId()) {
            *static_cast<float *>(ptr) = static_cast<float>(value->GetDouble());
        } else if (typeId == TypeInfo<double>::RegisteredId()) {
            *static_cast<double *>(ptr) = static_cast<double>(value->GetDouble());
        } else if (typeId == TypeInfo<std::string>::RegisteredId()) {
            *static_cast<std::string *>(ptr) = std::string(value->GetString());
        } else {
            const auto *node = GetTypeNode(typeId);
            SKY_ASSERT(node != nullptr && "type not registered");
            if (node == nullptr) {
                return;
            }

            if (node->serialization.jsonLoad != nullptr) {
                node->serialization.jsonLoad(ptr, *this);
                return;
            }

            if (node->info->staticInfo->isEnum) {
                LoadValueById(ptr, node->info->underlyingTypeId);
            } else {
                SKY_ASSERT(value != nullptr && value->IsObject());
                SKY_ASSERT(Start("classId"));
                auto id = Uuid::CreateFromString(LoadString());
                End();
                SKY_ASSERT(id == typeId);

                SKY_ASSERT(Start("elements"));
                for (const auto &member : node->members) {
                    std::string memberName = member.first.data();
                    auto *memberNode = GetTypeMember(memberName, typeId);
                    if (memberNode == nullptr) {
                        continue;
                    }

                    Any any = GetValueRawConst(ptr, typeId, memberName);
                    if (member.second.info->containerInfo != nullptr) {
                        auto *containerInfo = member.second.info->containerInfo;

                        auto size = StartArray(memberName);
                        if (containerInfo->sequenceView != nullptr) {
                            SequenceVisitor visitor(containerInfo, any.Data());
                            for (uint32_t i = 0; i < size; ++i) {
                                auto *arrayInst = visitor.Emplace();
                                LoadValueById(arrayInst, visitor.GetValueType());
                                NextArrayElement();
                            }
                        }
                        SetValueRaw(ptr, typeId, memberName, any.Data());
                        End();
                    } else if(Start(memberName)) {
                        LoadValueById(any.Data(), member.second.info->registeredId);
                        SetValueRaw(ptr, typeId, memberName, any.Data());
                        End();
                    }
                }
                End();
            }
        }
    }

    Any JsonInputArchive::LoadValueById(const Uuid &typeId)
    {
        auto res = MakeAny(typeId);
        LoadValueById(res.Data(), typeId);
        return res;
    }

    bool JsonInputArchive::LoadBool()
    {
        SKY_ASSERT(!stack.empty());
        const auto *value = stack.back();
        SKY_ASSERT(value != nullptr && value->IsBool());
        return value->GetBool();
    }

    int32_t JsonInputArchive::LoadInt()
    {
        SKY_ASSERT(!stack.empty());
        const auto *value = stack.back();
        SKY_ASSERT(value != nullptr && value->IsInt());

        return value->GetInt();
    }

    uint32_t JsonInputArchive::LoadUint()
    {
        SKY_ASSERT(!stack.empty());
        const auto *value = stack.back();
        SKY_ASSERT(value != nullptr && value->IsUint());

        return value->GetUint();
    }

    int64_t JsonInputArchive::LoadInt64()
    {
        SKY_ASSERT(!stack.empty());
        const auto *value = stack.back();
        SKY_ASSERT(value != nullptr && value->IsInt64());

        return value->GetInt64();
    }

    uint64_t JsonInputArchive::LoadUint64()
    {
        SKY_ASSERT(!stack.empty());
        const auto *value = stack.back();
        SKY_ASSERT(value != nullptr && value->IsUint64());

        return value->GetUint64();
    }

    double JsonInputArchive::LoadDouble()
    {
        SKY_ASSERT(!stack.empty());
        const auto *value = stack.back();
        SKY_ASSERT(value != nullptr && value->IsDouble());

        return value->GetDouble();
    }

    std::string JsonInputArchive::LoadString()
    {
        SKY_ASSERT(!stack.empty());
        const auto *value = stack.back();
        SKY_ASSERT(value != nullptr && value->IsString());

        return value->GetString();
    }

}
