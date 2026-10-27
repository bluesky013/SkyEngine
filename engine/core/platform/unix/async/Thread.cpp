//
// Created by blues on 2024/11/23.
//


#include <string_view>
#include <string>

#include <pthread.h>

namespace sky::impl {
    void SetCurrentThreadName(const std::string_view& u8Name)
    {
        std::string name(u8Name);
        if (name.size() > 15) {
            name.resize(15);
        }
#if defined(__APPLE__)
        pthread_setname_np(name.c_str());
#else
        pthread_setname_np(pthread_self(), name.c_str());
#endif
    }
} // namespace sky::impl
