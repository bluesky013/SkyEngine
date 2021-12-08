//
// Created by Zach Lee on 2021/12/9.
//

#pragma once

#include <framework/serialization/Type.h>

namespace sky {

    class Any {
    public:
        static constexpr uint32_t BLOCK_SIZE = 28;

        Any()
            : info(nullptr)
            , data{0}
        {
        }

        template <typename T, typename ...Args>
        Any(std::in_place_type_t<T>, Args&&...args)
            : data{0}
            , info(internal::TypeInfoNode<T>::Get()->RtInfo())
        {
            if (info->size > BLOCK_SIZE) {
                ptr = malloc(info->size);
            }
            auto instance = Data();
            new (instance) T{std::forward<Args>(args)...};
        }

        ~Any()
        {
            Destructor();
        }

        void* Data();

        template <typename T>
        T* GetAs()
        {
            if (info != nullptr && TypeInfo<T>::Hash() == info->typeId) {
                return static_cast<T*>(Data());
            }
            return nullptr;
        }

    private:

        void Destructor();

        union {
            uint8_t data[BLOCK_SIZE];
            void* ptr;
        };
        internal::TypeInfoRT* info = nullptr;
    };

}