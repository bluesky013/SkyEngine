//
// Created by Zach Lee on 2021/12/9.
//

#pragma once

#include <core/type/Rtti.h>
#include <core/type/TypeInfo.h>
#include <utility>

namespace sky {

    class Any {
    public:
        static constexpr uint32_t BLOCK_SIZE = 32 - sizeof(void *);

        Any() : data{0}, info(nullptr)
        {
        }

        template <typename T, typename... Args>
        explicit Any(std::in_place_type_t<T>, Args &&...args) : data{0}, info(TypeInfoObj<T>::Get()->RtInfo())
        {
            CheckMemory();
            new (Data()) T{std::forward<Args>(args)...};
        }

//        template <typename T>
//        explicit Any(std::reference_wrapper<T> ref) : Any(std::in_place_type<T *>, &ref.get())
//        {
//        }

        template <typename T>
        explicit Any(const T &t) : Any(std::in_place_type<T>, t)
        {
        }

        ~Any()
        {
            Destructor();
        }

        Any(const Any &any)
        {
            info = any.info;
            CheckMemory();
            Copy(any);
        }

        Any &operator=(const Any &any)
        {
            info = any.info;
            CheckMemory();
            Copy(any);
            return *this;
        }

        Any(Any &&any) noexcept
        {
            info = any.info;
            CheckMemory();
            Move(any);
        }

        Any &operator=(Any &&any) noexcept
        {
            info = any.info;
            CheckMemory();
            Move(any);
            return *this;
        }

        void *Data();

        const void *Data() const;

        template <typename T>
        T *GetAs()
        {
            return const_cast<T *>(std::as_const(*this).GetAsConst<T>());
        }

        template <typename T>
        const T *GetAsConst() const
        {
            if (info != nullptr &&
                    (TypeInfo<T>::RegisteredId() == info->registeredId ||
                    info->staticInfo->isEnum)) {
                return static_cast<const T *>(Data());
            }
            return nullptr;
        }

        const TypeInfoRT *Info() const
        {
            return info;
        }

        explicit operator bool() const
        {
            return Data() != nullptr;
        }

    private:
        void CheckMemory();
        void Destructor();
        void Move(Any &any);
        void Copy(const Any &any);

        union {
            uint8_t data[BLOCK_SIZE];
            void   *ptr;
        };
        const TypeInfoRT *info = nullptr;
    };
} // namespace sky