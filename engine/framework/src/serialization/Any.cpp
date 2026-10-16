//
// Created by Zach Lee on 2021/12/9.
//

#include <cstdlib>
#include <cstring>
#include <framework/serialization/Any.h>
#include <memory>
#include <new>

namespace sky {

    void *Any::Data()
    {
        if (info == nullptr) {
            return nullptr;
        }
        return info->staticInfo->size > BLOCK_SIZE ? ptr : &data[0];
    }

    const void *Any::Data() const
    {
        if (info == nullptr) {
            return nullptr;
        }
        return info->staticInfo->size > BLOCK_SIZE ? ptr : &data[0];
    }

    void Any::CheckMemory()
    {
        if (info != nullptr && info->staticInfo->size > BLOCK_SIZE) {
            ptr = malloc(info->staticInfo->size);
        }
    }

    void Any::Destructor()
    {
        if (info == nullptr) {
            return;
        }

        auto *instance = Data();
        if (instance != nullptr && info->destructor != nullptr) {
            info->destructor(instance);
        }

        if (info->staticInfo->size > BLOCK_SIZE && ptr != nullptr) {
            free(ptr);
            ptr = nullptr;
        }
        memset(data, 0, BLOCK_SIZE);
        info = nullptr;
    }

    void Any::Copy(const Any &any)
    {
        if (info != nullptr && info->copy != nullptr) {
            info->copy(any.Data(), Data());
        }
    }

    Any Any::Create(const TypeInfoRT *info, const void *value)
    {
        Any any;
        if (info == nullptr || value == nullptr) {
            any.info = info;
            return any;
        }

        const bool                                  heapAllocated = info->staticInfo->size > BLOCK_SIZE;
        std::unique_ptr<void, decltype(&std::free)> allocation(nullptr, &std::free);
        void                                       *storage = any.data;
        if (heapAllocated) {
            allocation.reset(std::malloc(info->staticInfo->size));
            if (allocation == nullptr) {
                throw std::bad_alloc();
            }
            storage = allocation.get();
        }
        if (info->copy != nullptr) {
            info->copy(value, storage);
        } else if (info->staticInfo != nullptr && info->staticInfo->isTrivial) {
            std::memcpy(storage, value, info->staticInfo->size);
        }
        // Publish the type only after construction. On a throwing placement
        // copy the local owner frees raw storage without destroying an object.
        if (heapAllocated) {
            any.ptr = allocation.release();
        }
        any.info = info;
        return any;
    }

    void Any::Move(Any &any)
    {
        if (any.info == nullptr) {
            info = nullptr;
            return;
        }

        if (any.info->staticInfo->size > BLOCK_SIZE) {
            ptr     = any.ptr;
            any.ptr = nullptr;
        } else {
            if (info != nullptr && info->move != nullptr) {
                info->move(any.Data(), Data());
            } else if (info != nullptr && info->copy != nullptr) {
                info->copy(any.Data(), Data());
            }
            any.Destructor();
        }
    }

} // namespace sky
