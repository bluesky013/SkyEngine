//
// Created by Zach Lee on 2021/12/3.
//

#pragma once

#include <core/hash/Fnv1a.h>
#include <core/util/Uuid.h>
#include <core/concept/Concept.h>
#include <string_view>
#include <type_traits>

#ifdef _MSC_VER
#define PRETTY_FUNC __FUNCSIG__
#else
#define PRETTY_FUNC __PRETTY_FUNCTION__
#endif

namespace sky {

    template <typename T>
    constexpr std::string_view PrettyFunc()
    {
        return PRETTY_FUNC;
    }


    constexpr std::string_view GetClassNameFromPrettyFunc(std::string_view prettyFunc)
    {
#ifdef _MSC_VER
        auto t1 = prettyFunc.find_first_of(' ', prettyFunc.find("PrettyFunc<")) + 1;
        return prettyFunc.substr(t1, prettyFunc.find_last_of('>') - t1);
#else
        auto t1 = prettyFunc.find_first_not_of(' ', prettyFunc.find_first_of('=') + 1);
        return prettyFunc.substr(t1, prettyFunc.find_first_of("];", t1) - t1);
#endif
    }

    template <typename T>
    constexpr std::string_view GetClassNameWithNameSpace()
    {
        static_assert(!std::is_fundamental_v<T>);
        return GetClassNameFromPrettyFunc(PrettyFunc<T>());
    }

    struct StaticTypeInfo {
        const size_t rank;
        const size_t size;
        const bool   isFundamental;
        const bool   isVoid;
        const bool   isNullptr;
        const bool   isArithmetic;
        const bool   isFloatingPoint;
        const bool   isInteger;
        const bool   isCompound;
        const bool   isPointer;
        const bool   isMemberObjectPointer;
        const bool   isMemberFunctionPointer;
        const bool   isArray;
        const bool   isEnum;
        const bool   isUnion;
        const bool   isClass;
        const bool   isTrivial;
        const bool   isSequenceContainer;
        const bool   isAssociativeContainer;
    };

    struct SequenceViewBase {
        virtual ~SequenceViewBase() = default;
        virtual size_t Count(void* ptr) const = 0;
        virtual void* Emplace(void* ptr) = 0;
        virtual void* GetByIndex(void* ptr, size_t idx) = 0;
        virtual void EraseByIndex(void* ptr, size_t idx) = 0;
    };

    template <typename T>
    struct SequenceView : public SequenceViewBase {
        using ValueType = typename T::value_type;

        size_t Count(void* ptr) const override
        {
            return static_cast<T*>(ptr)->size();
        }

        void* Emplace(void* ptr) override
        {
            auto *ct = static_cast<T*>(ptr);
            ct->insert(ct->end(), ValueType{});
            return &ct->back();
        }

        void EraseByIndex(void* ptr, size_t idx) override
        {
            auto *ct = static_cast<T*>(ptr);
            auto iter = ct->begin();
            std::advance(iter, idx);
            ct->erase(iter);
        }

        void* GetByIndex(void* ptr, size_t idx) override
        {
            auto *ct = static_cast<T*>(ptr);
            auto iter = ct->begin();
            std::advance(iter, idx);
            return &(*iter);
        }
    };

    struct ContainerInfo {
        SequenceViewBase* sequenceView = nullptr;
        Uuid valueType;
        Uuid keyType;
    };

    using DestructorDelete  = void (*)(void *ptr);
    using Destructor        = void (*)(void *ptr);
    using ConstructorNew    = void* (*)();
    using ConstructorPlace  = void (*)(void *ptr);
    using CopyFn            = void (*)(const void *src, void *dst);

    struct TypeInfoRT {
        std::string_view       name;
        Uuid                   registeredId;
        Uuid                   underlyingTypeId;
        const StaticTypeInfo*  staticInfo    = nullptr;
        ContainerInfo*         containerInfo = nullptr;
        ConstructorNew         newFunc       = nullptr; // default constructor
        ConstructorPlace       placeFunc     = nullptr; // default constructor
        DestructorDelete       deleteFunc    = nullptr; // default destructor
        Destructor             destructor    = nullptr; // default destructor
        CopyFn                 copy          = nullptr; // default copy constructor
    };

    // There may be different values on different platforms, limited to runtime and not persistent.
    template <typename T>
    static constexpr uint32_t RuntimeTypeId()
    {
        return Fnv1a32(PrettyFunc<T>());
    }

    template <typename T>
    struct TypeAllocate {
        static constexpr bool CTOR = std::is_default_constructible_v<T>;
        static constexpr bool DTOR = std::is_destructible_v<T>;
        static constexpr bool COPY = std::is_copy_constructible_v<T>;

        static void Construct(void *ptr)
        {
            if constexpr (CTOR) {
                new (ptr) T{};
            }
        }

        static void Destruct(void *ptr)
        {
            if constexpr (DTOR) {
                if (T *p = static_cast<T *>(ptr); p != nullptr) {
                    p->~T();
                }
            }
        }

        static void* New()
        {
            if constexpr (CTOR) {
                return new T();
            }
            return nullptr;
        }

        static void Delete(void *ptr)
        {
            if constexpr (DTOR) {
                if (T *p = static_cast<T *>(ptr); p != nullptr) {
                    delete p;
                }
            }
        }

        static void Copy(const void *src, void *dst)
        {
            if constexpr (COPY) {
                new(dst) T{*((T *) src)};
            }
        }
    };
} // namespace sky
