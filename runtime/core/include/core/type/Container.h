//
// Created by blues on 2024/10/5.
//

#pragma once

#include <type_traits>

namespace sky {

    template <typename T, typename Enable = void>
    struct CheckViewType : std::false_type {};

    template <typename T>
    struct CheckViewType<T, std::enable_if_t<std::is_same_v<typename T::iterator, typename T::const_iterator>>>
        : std::true_type {};

    struct ContainerTraitsBase {
        using YES = uint16_t;
        using NO  = uint8_t;

        template <class U>
        static constexpr YES CheckKeyType(typename U::key_type*);

        template <class U>
        static constexpr NO CheckKeyType(...);

        template <class U>
        static constexpr YES CheckValueType(typename U::value_type*);

        template <class U>
        static constexpr NO CheckValueType(...);

        template <class U>
        static constexpr YES CheckIterator(typename U::iterator*);

        template <class U>
        static constexpr NO CheckIterator(...);

        template <class U>
        static constexpr YES CheckConstIterator(typename U::const_iterator*);

        template <class U>
        static constexpr NO CheckConstIterator(...);

//        template <class U, typename = void>
//        static constexpr NO CheckViewType();
//
//        template <class U, typename std::enable_if_t<std::is_same_v<U::iterator, U::const_iterator>>>
//        static constexpr YES CheckViewType();
    };
    
    template <class T>
    struct ContainerTraits : ContainerTraitsBase
    {
        static constexpr bool IS_ASSOCIATIVE = sizeof(CheckValueType<T>(nullptr)) == sizeof(YES) &&
            sizeof(CheckIterator<T>(nullptr)) == sizeof(YES) &&
            sizeof(CheckConstIterator<T>(nullptr)) == sizeof(YES) &&
            sizeof(CheckKeyType<T>(nullptr)) == sizeof(YES) &&
            !CheckViewType<T>::value;

        static constexpr bool IS_SEQUENCE = sizeof(CheckValueType<T>(nullptr)) == sizeof(YES) &&
            sizeof(CheckIterator<T>(nullptr)) == sizeof(YES) &&
            sizeof(CheckConstIterator<T>(nullptr)) == sizeof(YES) &&
            sizeof(CheckKeyType<T>(nullptr)) == sizeof(NO) &&
            !CheckViewType<T>::value;
    };

    template <class T>
    struct SequenceContainerMap {
        using ValueType = T::value_type;
    };

} // namespace sky
