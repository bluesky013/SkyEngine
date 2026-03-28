//
// Created by blues on 2025/5/25.
//

#include <core/console/CVar.h>
#include <core/console/CommandRegistry.h>

namespace sky {

    // Explicit template instantiations for supported types
    template <typename T>
    CVar<T>::CVar(std::string_view name, const T &defaultVal, std::string_view desc, CVarFlags flags)
        : name_(name)
        , desc_(desc)
        , category_(detail::ExtractCategory(name_))
        , flags_(flags)
        , value_(defaultVal)
        , default_(defaultVal)
    {
        CommandRegistry::Get()->RegisterCVar(this);
    }

    template <typename T>
    CVar<T>::~CVar()
    {
        auto *registry = CommandRegistry::Get();
        if (registry != nullptr) {
            registry->UnregisterCVar(this);
        }
    }

    template class CVar<bool>;
    template class CVar<int>;
    template class CVar<float>;
    template class CVar<double>;
    template class CVar<std::string>;

} // namespace sky
