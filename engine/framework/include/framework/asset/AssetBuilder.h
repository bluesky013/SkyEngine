//
// Created by Zach Lee on 2023/2/20.
//

#pragma once
#include <core/file/FileSystem.h>
#include <core/platform/Platform.h>
#include <core/util/String.h>
#include <core/util/Uuid.h>
#include <framework/asset/Asset.h>
#include <framework/asset/AssetCommon.h>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace sky {

    // Sparse per-target override: only the keys the author changed (unset keys fall back to the preset).
    using BuildSettingsOverride = std::map<std::string, std::string>;

    class AssetBuilder {
    public:
        AssetBuilder()          = default;
        virtual ~AssetBuilder() = default;

        virtual const std::vector<std::string> &GetExtensions() const = 0;

        virtual void Import(const AssetImportRequest &request) const
        {
        }

        virtual Any RequireImportSetting(const FilePath &request) const
        {
            return Any{};
        }

        virtual void Request(const AssetBuildRequest &request, AssetBuildResult &result)
        {
        }

        virtual std::string_view QueryType(const std::string &ext) const
        {
            return "";
        }

        // Human-readable effective settings this builder would apply for a product bundle, as
        // ordered key/value pairs (e.g. encode, srgb, max size, mip generation). Default: none.
        virtual std::vector<std::pair<std::string, std::string>> DescribeSettings(const ProductBundleKey &bundle) const
        {
            return {};
        }

        // Reflected type describing this builder's per-target cook settings, or nullptr when the
        // builder has none. The editor binds an instance of it to the generic reflected form.
        virtual const TypeInfoRT *GetSettingsType() const
        {
            return nullptr;
        }

        // Materialize the effective settings for a bundle (the bundle preset overlaid with the sparse
        // per-asset override) into a reflected Any of GetSettingsType(). Empty when the builder has no
        // settings type.
        virtual Any MakeSettings(const ProductBundleKey &bundle, const BuildSettingsOverride &override) const
        {
            return Any{};
        }

        // Sparse override that reproduces `edited` (only the keys differing from the bundle preset), so
        // the editor can persist a reflected-form edit as a manifest `cook.settings[target]` block.
        virtual BuildSettingsOverride DiffSettings(const ProductBundleKey &bundle, const Any &edited) const
        {
            return {};
        }

        // Effective settings for a bundle with a per-asset sparse override applied. Default: ignore the
        // override and report the preset-only description.
        virtual std::vector<std::pair<std::string, std::string>> DescribeSettings(const ProductBundleKey      &bundle,
                                                                                  const BuildSettingsOverride &override) const
        {
            return DescribeSettings(bundle);
        }

        virtual void LoadConfig(const FileSystemPtr &cfg)
        {
        }
    };
} // namespace sky
