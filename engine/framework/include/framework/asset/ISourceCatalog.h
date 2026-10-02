//
// Created by blues on 2026/10/2.
//

#pragma once

#include <string>
#include <core/util/Uuid.h>

namespace sky {

    // Editor/authoring-side source catalog queried by the loader (D3).
    // The runtime injects an empty implementation, so path lookups only use the product index.
    class ISourceCatalog {
    public:
        virtual ~ISourceCatalog() = default;

        virtual bool ResolvePath(const std::string &path, Uuid &out) const = 0;
        virtual bool Exists(const Uuid &id) const = 0;
        virtual bool GetType(const Uuid &id, std::string &out) const = 0;
        virtual bool GetTarget(const Uuid &id, std::string &out) const = 0;
        virtual bool GetSourcePath(const Uuid &id, std::string &out) const = 0;
    };

} // namespace sky
