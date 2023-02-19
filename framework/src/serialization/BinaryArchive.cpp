//
// Created by Zach Lee on 2023/2/19.
//

#include <framework/serialization/BinaryArchive.h>
#include <framework/serialization/SerializationContext.h>

namespace sky {

    void BinaryInputArchive::LoadObject(void *ptr, uint32_t typeId)
    {
        if (typeId == TypeInfo<bool>::Hash()) {
            LoadValue(*static_cast<bool*>(ptr));
        } else if (typeId == TypeInfo<uint64_t>::Hash()) {
            LoadValue(*static_cast<uint64_t*>(ptr));
        } else if (typeId == TypeInfo<uint32_t>::Hash()) {
            LoadValue(*static_cast<uint32_t*>(ptr));
        } else if (typeId == TypeInfo<uint16_t>::Hash()) {
            LoadValue(*static_cast<uint16_t*>(ptr));
        } else if (typeId == TypeInfo<uint8_t>::Hash()) {
            LoadValue(*static_cast<uint8_t*>(ptr));
        } else if (typeId == TypeInfo<int64_t>::Hash()) {
            LoadValue(*static_cast<int64_t*>(ptr));
        } else if (typeId == TypeInfo<int32_t>::Hash()) {
            LoadValue(*static_cast<int32_t*>(ptr));
        } else if (typeId == TypeInfo<int16_t>::Hash()) {
            LoadValue(*static_cast<int16_t*>(ptr));
        } else if (typeId == TypeInfo<int8_t>::Hash()) {
            LoadValue(*static_cast<int8_t*>(ptr));
        } else if (typeId == TypeInfo<float>::Hash()) {
            LoadValue(*static_cast<float*>(ptr));
        } else if (typeId == TypeInfo<double>::Hash()) {
            LoadValue(*static_cast<double*>(ptr));
        } else if (typeId == TypeInfo<std::string>::Hash()) {
            LoadValue(*static_cast<std::string*>(ptr));
        } else {
            auto node = GetTypeNode(typeId);
            SKY_ASSERT(node != nullptr && "type not registered");
            if (node == nullptr) {
                return;
            }

            if (node->serialization.binaryLoad != nullptr) {
                node->serialization.binaryLoad(ptr, *this);
                return;
            }
            uint32_t id = 0, size = 0;
            LoadValue(id);
            LoadValue(size);
            SKY_ASSERT(id == typeId);

            for (auto &member : node->members) {
                std::string memberName = member.first.data();
            }
        }
    }

    void BinaryOutputArchive::SaveObject(const void* ptr, uint32_t typeId)
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

        }
    }
}