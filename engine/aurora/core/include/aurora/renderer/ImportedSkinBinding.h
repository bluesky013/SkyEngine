#pragma once

#include <core/util/Uuid.h>
#include <map>
#include <string>

namespace sky::aurora {
    struct ImportedSkinBinding {
        Uuid skin;
        Uuid skeleton;
        bool operator==(const ImportedSkinBinding &) const = default;
    };

    enum class ImportedSkinBindingStatus { Ready, Missing, Ambiguous };

    // Import defaults are authoring hints; successful selection becomes explicit component references.
    inline ImportedSkinBindingStatus
    ResolveImportedSkinBinding(const Uuid &mesh, const std::map<std::string, Uuid> &defaults, ImportedSkinBinding &out)
    {
        if (!mesh)
            return ImportedSkinBindingStatus::Missing;
        ImportedSkinBinding candidate;
        bool                found      = false;
        bool                hasBinding = false;
        for (const auto &[key, value] : defaults) {
            if (!key.starts_with("binding:") || !key.ends_with(":mesh") || value != mesh)
                continue;
            hasBinding        = true;
            const auto prefix = key.substr(0, key.size() - 4);
            const auto skin = defaults.find(prefix + "skin"), skeleton = defaults.find(prefix + "skeleton");
            if (skin == defaults.end() || skeleton == defaults.end() || !skin->second || !skeleton->second)
                return ImportedSkinBindingStatus::Missing;
            const ImportedSkinBinding binding{skin->second, skeleton->second};
            if (found && candidate != binding)
                return ImportedSkinBindingStatus::Ambiguous;
            candidate = binding;
            found     = true;
        }
        if (!hasBinding) {
            const auto geometry = defaults.find("mesh"), skin = defaults.find("skin"), skeleton = defaults.find("skeleton");
            if (geometry == defaults.end() || geometry->second != mesh || skin == defaults.end() || skeleton == defaults.end() || !skin->second ||
                !skeleton->second)
                return ImportedSkinBindingStatus::Missing;
            candidate = {skin->second, skeleton->second};
        }
        out = candidate;
        return ImportedSkinBindingStatus::Ready;
    }
} // namespace sky::aurora
