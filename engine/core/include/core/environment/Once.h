//
// Process-wide one-shot guard for idempotent setup/registration.
//
// A function-local `static bool` guard is per-DLL: this engine links Core/Framework static
// libraries into every module DLL, so each DLL gets its own copy and the guard only works
// within one DLL. When the guarded work touches process-shared state (e.g. SerializationContext
// type registration, a backend registry), use this instead - the instance lives in the shared
// Environment, so "run once" is true across all modules.
//

#pragma once

#include <core/environment/Singleton.h>

#include <string>
#include <string_view>
#include <unordered_set>

namespace sky {

    class Once : public Singleton<Once> {
    public:
        // Returns true the first time `key` is seen across every module in the process.
        bool Run(std::string_view key);

    private:
        friend class Singleton<Once>;

        std::unordered_set<std::string> keys;
    };

} // namespace sky
