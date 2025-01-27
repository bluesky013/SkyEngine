//
// Created by blues on 2024/11/23.
//

#include <core/name/Name.h>
#include "NameDataBase.h"

#include <core/hash/Crc32.h>
namespace sky {

    uint32_t Name::Hash(const char* ch, uint32_t length) noexcept
    {
        return Crc32::Cal(reinterpret_cast<const uint8_t*>(ch), length);
    }

    Name::Name() : handle(0)
    {
    }

    Name::Name(const char *ch)
        : handle(NameDataBase::Get()->FetchOrRegister(ch))
    {
#if _DEBUG
        view = NameDataBase::Get()->GetStr(handle);
#endif
    }

    std::string_view Name::GetStr() const noexcept
    {
#if _DEBUG
        return view;
#else
        return NameDataBase::Get()->GetStr(handle);
#endif
    }

    bool Name::Equals(Name A, std::string_view B) noexcept
    {
        return Equals(A, Name(B.data()));
    }

    bool Name::Equals(Name A, const char* B) noexcept
    {
        return Equals(A, Name(B));
    }

    bool Name::Equals(Name A, Name B) noexcept
    {
        return A == B;
    }
} // namespace sky