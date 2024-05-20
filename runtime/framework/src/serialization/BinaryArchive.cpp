//
// Created by Zach Lee on 2023/2/19.
//

#include <framework/serialization/BinaryArchive.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/SerializationUtil.h>

namespace sky {

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
            for (const auto &member : node->members) {
                std::string memberName = member.first.data();
                Any value = GetValueRaw(ptr, typeId, memberName);
                LoadObject(value.Data(), member.second.info->registeredId);
                SetValueRaw(ptr, typeId, memberName, value.Data());
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

            for (const auto &member : node->members) {
                std::string memberName = member.first.data();
                Any value = GetValueRawConst(ptr, typeId, memberName);
                SaveObject(value.Data(), member.second.info->registeredId);
            }
        }
    }
}