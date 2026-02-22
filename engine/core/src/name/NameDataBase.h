//
// Created by blues on 2024/11/22.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/name/NameTypes.h>

#include <unordered_map>
#include <mutex>
#include <vector>
#include <memory>

#ifndef NAME_COLLISION_DETECT
#define NAME_COLLISION_DETECT 1
#endif

namespace sky {
    class NameAllocator;

    class NameDataBase : public Singleton<NameDataBase> {
    public:
        NameDataBase();
        ~NameDataBase() override = default;

        NameEntryHandle FetchOrRegister(const char* ch);

        std::string_view GetStr(NameEntryHandle handle) const;
    private:
        std::string_view Visit(uint32_t index) const;

        mutable std::mutex mutex;
        std::unique_ptr<NameAllocator, void(*)(NameAllocator*)> allocator;

        std::vector<NameEntry> entries;
        std::unordered_map<uint32_t, uint32_t> nameHashMap;
    };

} // namespace sky
