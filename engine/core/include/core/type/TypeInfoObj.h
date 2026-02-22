//
// Created by blues on 2024/5/5.
//

#pragma once
#include <core/environment/Singleton.h>
#include <core/memory/Allocator.h>
#include <core/type/Type.h>
#include <core/type/Container.h>
#include <core/platform/Platform.h>

namespace sky {
    template <typename T>
    class TypeInfoObj;

    template <typename T>
    ContainerInfo* ContainerInfoStatic()
    {
        if constexpr (ContainerTraits<T>::IS_SEQUENCE) {
            static SequenceView<T> view;
            static ContainerInfo info = { &view };
            return &info;
        }
        return nullptr;
    }

    template <typename T>
    class TypeInfoObj : public Singleton<TypeInfoObj<T>> {
    public:
        const TypeInfoRT *Register(std::string_view name, const Uuid &uuid, const Uuid &underlyingId = {})
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (info == nullptr) {
                info = new TypeInfoRT {
                    name,
                    uuid,
                    underlyingId,
                    &TypeInfoStatic<T>(),
                    ContainerInfoStatic<T>(),
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
                    {},
                    &TypeInfoStatic<T>(),
                    ContainerInfoStatic<T>(),
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