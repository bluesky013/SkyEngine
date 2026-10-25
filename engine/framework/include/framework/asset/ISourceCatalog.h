//
// Created by blues on 2026/10/2.
//

#pragma once

#include <core/util/Uuid.h>
#include <framework/asset/GeneratedAssetCatalog.h>
#include <string>

namespace sky {

    // Editor/authoring-side source catalog queried by the loader (D3).
    // The runtime injects an empty implementation, so path lookups only use the product index.
    class ISourceCatalog {
    public:
        virtual ~ISourceCatalog() = default;

        virtual bool ResolvePath(const std::string &path, Uuid &out) const = 0;
        virtual bool Exists(const Uuid &id) const                          = 0;
        virtual bool GetType(const Uuid &id, std::string &out) const       = 0;
        virtual bool GetTarget(const Uuid &id, std::string &out) const     = 0;
        virtual bool GetSourcePath(const Uuid &id, std::string &out) const = 0;
        virtual bool GetOwner(const Uuid &id, Uuid &out) const
        {
            if (!Exists(id)) {
                return false;
            }
            out = id;
            return true;
        }
        virtual bool GetGenerated(const Uuid &, GeneratedAssetRecord &) const
        {
            return false;
        }
        virtual bool RefreshOwner(const Uuid &)
        {
            return true;
        }
    };

} // namespace sky
