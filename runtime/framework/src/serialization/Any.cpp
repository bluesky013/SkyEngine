//
// Created by Zach Lee on 2021/12/9.
//

#include <framework/serialization/Any.h>

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
        if (info->staticInfo->size > BLOCK_SIZE) {
            ptr = malloc(info->staticInfo->size);
        }
    }

    void Any::Destructor()
    {
        auto *instance = Data();
        if (info == nullptr || instance == nullptr) {
            return;
        }

        if (info->destructor != nullptr) {
            info->destructor(instance);
        }

        if (info->staticInfo->size > BLOCK_SIZE && ptr != nullptr) {
            free(ptr);
        }
        memset(data, 0, BLOCK_SIZE);
    }

    void Any::Copy(const Any &any)
    {
        if (info != nullptr && info->copy != nullptr) {
            info->copy(any.Data(), Data());
        }
    }

    void Any::Move(Any &any)
    {
        if (info->staticInfo->size > BLOCK_SIZE) {
            ptr     = any.ptr;
            any.ptr = nullptr;
        } else {
            Copy(any);
            any.Destructor();
        }
    }

} // namespace sky