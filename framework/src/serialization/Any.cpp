//
// Created by Zach Lee on 2021/12/9.
//

#include <framework/serialization/Any.h>

namespace sky {

    void* Any::Data()
    {
        return info->size > BLOCK_SIZE ? ptr : &data[0];
    }


    void Any::Destructor()
    {
        if (info == nullptr) {
            return;
        }
        auto instance = Data();
        if (instance == nullptr) {
            return;
        }

        if (info->size > BLOCK_SIZE && ptr != nullptr) {
            free(ptr);
            ptr = nullptr;
        }

    }

}