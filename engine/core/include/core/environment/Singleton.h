//
// Created by Zach Lee on 2021/12/4.
//

#pragma once

#include <core/environment/Environment.h>
#include <core/type/Rtti.h>

namespace sky {

    template <typename T>
    class Singleton {
    public:
        static constexpr uint32_t ID = RuntimeTypeId<T>();

        static T *Get()
        {
            return GetPtr(true);
        }

        static void Destroy()
        {
            delete static_cast<T *>(Environment::Get()->UnRegister(ID));
        }

    protected:
        static T *GetPtr(bool create)
        {
            auto *environment = Environment::Get();
            // Resident DLLs must not keep private pointers to instances destroyed by another DLL.
            return static_cast<T *>(create ? environment->FindOrCreate(ID, []() -> void * { return new T(); }) : environment->Find(ID));
        }

        Singleton()          = default;
        virtual ~Singleton() = default;
    };

} // namespace sky
