//
// Created by blues on 2024/11/23.
//

#include <windows.h>
#include <string_view>

namespace sky::impl {

    void SetCurrentThreadName(const std::string_view& u8Name)
    {
        size_t size = MultiByteToWideChar(CP_UTF8, 0,
            u8Name.data(), static_cast<int>(u8Name.size()),
            nullptr, 0);
 
        std::wstring u16Name;
        u16Name.resize(size);
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            u8Name.data(), static_cast<int>(u8Name.size()),
            u16Name.data(), static_cast<int>(u16Name.size()));

        SetThreadDescription(GetCurrentThread(), u16Name.data());
    }
} // namespace sky::impl