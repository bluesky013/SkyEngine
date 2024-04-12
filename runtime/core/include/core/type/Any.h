//
// Created by Zach Lee on 2021/12/9.
//

#pragma once

#include <core/type/Rtti.h>
#include <core/type/Type.h>

namespace sky {

    class Any {
    public:
        static constexpr uint32_t BLOCK_SIZE = 32 - sizeof(void *);

        Any() : info(nullptr), data{0}
        {
        }

        template <typename T, typename... Args>
        Any(std::in_place_type_t<T>, Args &&...args) : data{0}, info(TypeInfoObj<T>::Get()->RtInfo())
        {
            CheckMemory();
            new (Data()) T{std::forward<Args>(args)...};
        }

        template <typename T>
        Any(std::reference_wrapper<T> ref) : Any(std::in_place_type<T *>, &ref.get())
        {
        }

        template <typename T>
        Any(const T &t) : Any(std::in_place_type<T>, t)
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

        Any(Any &&any)
        {
            info = any.info;
            CheckMemory();
            Move(any);
        }

        Any &operator=(Any &&any)
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
            if (info != nullptr && TypeInfo<T>::Hash() == info->typeId) {
                return static_cast<const T *>(Data());
            }
            return nullptr;
        }

        const TypeInfoRT *Info() const
        {
            return info;
        }

        operator bool() const
        {
            return Data() != nullptr;
        }

    private:
        void CheckMemory();

        void Construct();

        void Destructor();

        void Move(Any &any);

        void Copy(const Any &any);

        union {
            uint8_t data[BLOCK_SIZE];
            void   *ptr;
        };
        TypeInfoRT *info = nullptr;
    };
} // namespace sky