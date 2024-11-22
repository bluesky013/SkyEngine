//
// Created by blues on 2024/11/22.
//

#include "NameDataBase.h"

#include <core/platform/Platform.h>
#include <core/name/Name.h>

#include "NameAllocator.h"

namespace sky {
    NameDataBase::NameDataBase() : allocator(new NameAllocator(), [](NameAllocator *ptr) { delete ptr; })
    {
    }

    NameEntryHandle NameDataBase::FetchOrRegister(const char* ch)
    {
        auto length = strlen(ch);
        SKY_ASSERT(length + 1 <= NameAllocator::MAX_NAME_LEN);
        NameEntryHandle handle = Name::Hash(ch, static_cast<uint32_t>(length));

        std::lock_guard<std::mutex> lock(mutex);
        auto iter = nameHashMap.find(handle);
        if (iter != nameHashMap.end()) {
#if NAME_COLLISION_DETECT
            SKY_ASSERT(Visit(iter->second) == std::string_view(ch))
#endif
            return handle;
        }

        auto index = static_cast<NameEntryHandle>(entries.size());

        entries.emplace_back();
        auto &entry = entries.back();
        entry.length = static_cast<uint16_t>(length);
        entry.handle = allocator->Allocate(entry.length + 1); // add terminator
        memcpy(allocator->Visit(entry.handle), ch, length);

        nameHashMap.emplace(handle, index);
        return handle;
    }

    std::string_view NameDataBase::GetStr(NameEntryHandle handle) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto iter = nameHashMap.find(handle);
        if (iter != nameHashMap.end()) {
            return Visit(iter->second);
        }

        return {};
    }

    std::string_view NameDataBase::Visit(uint32_t index) const
    {
        const auto &entry = entries[index];
        return std::string_view { allocator->Visit(entry.handle), entry.length };
    }

} // namespace sky