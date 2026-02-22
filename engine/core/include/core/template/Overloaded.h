//
// Created by Zach Lee on 2023/4/15.
//

#pragma once

namespace sky {

    template <class... Ts>
    struct Overloaded : Ts... {
        using Ts::operator()...;
    };

    template <class... Ts>
    Overloaded(Ts...) -> Overloaded<Ts...>;

} // namespace sky