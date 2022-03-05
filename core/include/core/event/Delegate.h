//
// Created by Zach Lee on 2022/3/6.
//

#pragma once
#include <functional>

namespace sky {

    template <typename>
    class Delegate;

    template <typename Ret, typename ...Args>
    class Delegate<Ret(Args...)> {
    public:
        using FuncType = Ret(void*, Args...);

        Delegate() = default;
        ~Delegate() = default;

        template <auto F>
        void Connect()
        {
            func = [](void*, Args... args) -> Ret {
                return Ret(std::invoke(F, std::forward<Args>(args)...));
            };
        }

        Ret operator()(Args... args)
        {
            return func(data, std::forward<Args>(args)...);
        }

        void Reset()
        {
            data = nullptr;
            func = nullptr;
        }

    private:
        void* data = nullptr;
        FuncType* func = nullptr;
    };

}
