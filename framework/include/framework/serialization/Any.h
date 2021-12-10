//
// Created by Zach Lee on 2021/12/9.
//

#pragma once

#include <framework/serialization/Type.h>

namespace sky {

    class Any {
    public:
        static constexpr uint32_t BLOCK_SIZE = 32 - sizeof(void*);

        Any()
            : info(nullptr)
            , data{0}
        {
        }

        template <typename T, typename ...Args>
        Any(std::in_place_type_t<T>, Args&&...args)
            : data{0}
            , info(TypeInfoObj<T>::Get()->RtInfo())
        {
            Construct();
            new (Data()) T{std::forward<Args>(args)...};
        }

        template <typename T>
        Any(const T& t) : Any(std::in_place_type<T>, t)
        {
        }

        ~Any()
        {
            Destructor();
        }

        Any(const Any& any)
        {
            info = any.info;
            Construct();
            Copy(any);
        }

        Any& operator=(const Any& any)
        {
            info = any.info;
            Construct();
            return *this;
        }

        Any(Any&& any)
        {
            info = any.info;
            Move(any);
        }

        Any& operator=(Any&& any)
        {
            info = any.info;
            Move(any);
            return *this;
        }

        void* Data();

        const void* Data() const;

        template <typename T>
        T* GetAs()
        {
            return const_cast<T*>(std::as_const(*this).GetAsConst<T>());
        }

        template <typename T>
        const T* GetAsConst() const
        {
            if (info != nullptr && TypeInfo<T>::Hash() == info->hash) {
                return static_cast<const T*>(Data());
            }
            return nullptr;
        }

        bool Set(const std::string& str, const Any& any);

        Any Get(const std::string& str);

    private:
        void Construct();

        void Destructor();

        void Move(Any& any);

        void Copy(const Any& any);

        union {
            uint8_t data[BLOCK_SIZE];
            void* ptr;
        };
        TypeInfoRT* info = nullptr;
    };

}