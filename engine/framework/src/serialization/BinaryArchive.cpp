//
// Created by Zach Lee on 2023/2/19.
//

#include <framework/serialization/BinaryArchive.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/SerializationUtil.h>
#include <core/logger/Logger.h>

namespace sky {

    static const char *TAG = "BinaryArchive";

    void BinaryInputArchive::LoadObject(void *ptr, const Uuid &typeId)
    {
        if (typeId == TypeInfo<bool>::RegisteredId()) {
            LoadValue(*static_cast<bool*>(ptr));
        } else if (typeId == TypeInfo<uint64_t>::RegisteredId()) {
            LoadValue(*static_cast<uint64_t*>(ptr));
        } else if (typeId == TypeInfo<uint32_t>::RegisteredId()) {
            LoadValue(*static_cast<uint32_t*>(ptr));
        } else if (typeId == TypeInfo<uint16_t>::RegisteredId()) {
            LoadValue(*static_cast<uint16_t*>(ptr));
        } else if (typeId == TypeInfo<uint8_t>::RegisteredId()) {
            LoadValue(*static_cast<uint8_t*>(ptr));
        } else if (typeId == TypeInfo<int64_t>::RegisteredId()) {
            LoadValue(*static_cast<int64_t*>(ptr));
        } else if (typeId == TypeInfo<int32_t>::RegisteredId()) {
            LoadValue(*static_cast<int32_t*>(ptr));
        } else if (typeId == TypeInfo<int16_t>::RegisteredId()) {
            LoadValue(*static_cast<int16_t*>(ptr));
        } else if (typeId == TypeInfo<int8_t>::RegisteredId()) {
            LoadValue(*static_cast<int8_t*>(ptr));
        } else if (typeId == TypeInfo<char>::RegisteredId()) {
            LoadValue(*static_cast<char*>(ptr));
        } else if (typeId == TypeInfo<float>::RegisteredId()) {
            LoadValue(*static_cast<float*>(ptr));
        } else if (typeId == TypeInfo<double>::RegisteredId()) {
            LoadValue(*static_cast<double*>(ptr));
        } else if (typeId == TypeInfo<std::string>::RegisteredId()) {
            LoadValue(*static_cast<std::string*>(ptr));
        } else {
            const auto *node = GetTypeNode(typeId);
            SKY_ASSERT(node != nullptr && "type not registered");
            if (node == nullptr) {
                return;
            }

            if (node->serialization.binaryLoad != nullptr) {
                node->serialization.binaryLoad(ptr, *this);
                return;
            }

            if (node->info->staticInfo->isEnum) {
                LoadObject(ptr, node->info->underlyingTypeId);
                return;
            }

            for (const auto &member : node->members) {
                std::string memberName = member.first.data();
                const auto *info = member.second.info;

                if (info->registeredId == TypeInfo<SequenceVisitor>::RegisteredId()) {
                    LOG_W(TAG, "member '%s' of type '%s' is not serializable", memberName.c_str(), node->info->name.data());
                    continue;
                }

                Any value = GetValueRaw(ptr, typeId, memberName);
                if (info->containerInfo != nullptr && info->containerInfo->valueType != TypeInfo<char>::RegisteredId()) {
                    auto *containerInfo = info->containerInfo;
                    if (containerInfo->sequenceView != nullptr) {
                        uint32_t count = 0;
                        LoadValue(count);
                        SequenceVisitor visitor(containerInfo, value.Data());
                        for (uint32_t i = 0; i < count; ++i) {
                            auto *element = visitor.Emplace();
                            LoadObject(element, visitor.GetValueType());
                        }
                        SetValueRaw(ptr, typeId, memberName, value.Data());
                    } else {
                        LOG_W(TAG, "container member '%s' of type '%s' is not serializable", memberName.c_str(), node->info->name.data());
                    }
                } else {
                    LoadObject(value.Data(), info->registeredId);
                    SetValueRaw(ptr, typeId, memberName, value.Data());
                }
            }
        }
    }

    void BinaryOutputArchive::SaveObject(const void* ptr, const Uuid &typeId)
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
        } else if (typeId == TypeInfo<char>::RegisteredId()) {
            SaveValue(*static_cast<const char *>(ptr));
        } else if (typeId == TypeInfo<float>::RegisteredId()) {
            SaveValue(*static_cast<const float *>(ptr));
        } else if (typeId == TypeInfo<double>::RegisteredId()) {
            SaveValue(*static_cast<const double *>(ptr));
        } else if (typeId == TypeInfo<std::string>::RegisteredId()) {
            SaveValue(*static_cast<const std::string *>(ptr));
        } else {
            const auto *node = GetTypeNode(typeId);
            SKY_ASSERT(node != nullptr && "type not registered");
            if (node == nullptr) {
                return;
            }

            if (node->serialization.binarySave != nullptr) {
                node->serialization.binarySave(ptr, *this);
                return;
            }

            if (node->info->staticInfo->isEnum) {
                SaveObject(ptr, node->info->underlyingTypeId);
                return;
            }

            for (const auto &member : node->members) {
                std::string memberName = member.first.data();
                const auto *info = member.second.info;

                if (info->registeredId == TypeInfo<SequenceVisitor>::RegisteredId()) {
                    LOG_W(TAG, "member '%s' of type '%s' is not serializable", memberName.c_str(), node->info->name.data());
                    continue;
                }

                Any value = GetValueRawConst(ptr, typeId, memberName);
                if (info->containerInfo != nullptr && info->containerInfo->valueType != TypeInfo<char>::RegisteredId()) {
                    auto *containerInfo = info->containerInfo;
                    if (containerInfo->sequenceView != nullptr) {
                        SequenceVisitor visitor(containerInfo, value.Data());
                        auto count = static_cast<uint32_t>(visitor.Count());
                        SaveValue(count);
                        for (uint32_t i = 0; i < count; ++i) {
                            SaveObject(visitor.GetByIndex(i), visitor.GetValueType());
                        }
                    } else {
                        LOG_W(TAG, "container member '%s' of type '%s' is not serializable", memberName.c_str(), node->info->name.data());
                    }
                } else {
                    SaveObject(value.Data(), info->registeredId);
                }
            }
        }
    }
}
