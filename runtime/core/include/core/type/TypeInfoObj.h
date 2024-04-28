//
// Created by blues on 2024/5/5.
//

#pragma once
#include <core/environment/Singleton.h>
#include <core/memory/Allocator.h>
#include <core/type/Type.h>
#include <core/platform/Platform.h>

namespace sky {

    template <typename T>
    class TypeInfoObj : public Singleton<TypeInfoObj<T>> {
    public:
        const TypeInfoRT *Register(std::string_view name, const Uuid &uuid)
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (info == nullptr) {
                info = new TypeInfoRT {
                        name,
                        uuid,
                        &TypeInfoStatic<T>(),
                        TypeAllocate<T>::CTOR ? &TypeAllocate<T>::New : nullptr,
                        TypeAllocate<T>::CTOR ? &TypeAllocate<T>::Construct : nullptr,
                        TypeAllocate<T>::DTOR ? &TypeAllocate<T>::Delete : nullptr,
                        TypeAllocate<T>::DTOR ? &TypeAllocate<T>::Destruct : nullptr,
                        TypeAllocate<T>::COPY ? &TypeAllocate<T>::Copy : nullptr
                };
            } else {
                info->name = name;
                info->registeredId = uuid;
            }
            return info;
        }

        const TypeInfoRT *RtInfo()
        {
            if (info == nullptr) {
                info = new TypeInfoRT {
                        "",
                        {},
                        &TypeInfoStatic<T>(),
                        TypeAllocate<T>::CTOR ? &TypeAllocate<T>::New : nullptr,
                        TypeAllocate<T>::CTOR ? &TypeAllocate<T>::Construct : nullptr,
                        TypeAllocate<T>::DTOR ? &TypeAllocate<T>::Delete : nullptr,
                        TypeAllocate<T>::DTOR ? &TypeAllocate<T>::Destruct : nullptr,
                        TypeAllocate<T>::COPY ? &TypeAllocate<T>::Copy : nullptr
                };
            }
            return info;
        }

    private:
        std::mutex  mutex;
        TypeInfoRT *info = nullptr;
    };

} // namespace sky