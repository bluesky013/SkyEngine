//
// Created by blues on 2026/2/17.
//

#pragma once

namespace sky {

    template <typename T, typename = void>
    struct IsBinarySerializable : std::false_type {};

    template <typename T>
    struct IsBinarySerializable<T, std::void_t<typename T::BinarySerializable>> : std::true_type {};

    template <typename T>
    constexpr bool is_binary_serializable_v = IsBinarySerializable<T>::value;

} // namespace sky
