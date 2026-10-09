//
// Created by blues on 2024/6/16.
//

#pragma once

#include <core/event/Event.h>
#include <core/file/FileSystem.h>
#include <core/template/ReferenceObject.h>
#include <core/util/Uuid.h>
#include <framework/serialization/Any.h>
#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace sky {

    // A source mount: an ordered entry of the logical namespace. Sources resolve against mounts in
    // order (earlier mounts shadow later ones). Provided by the framework so consumers do not probe
    // filesystems or hardcode mount names.
    struct AssetMount {
        std::string id;          // stable id, e.g. "workspace", "engine"
        std::string displayName; // human-readable label, e.g. "Project", "Engine"
        bool        writable = false;
    };

    // asset source info
    struct AssetSourceInfo : public RefObject {
        FilePath          path;         // logical source path within the mounted namespace
        std::string       name;         // marked name used to load by name, can be empty
        std::string       ext;          // file extension (source of the derived AssetTypeId)
        std::string       mount;        // owning mount id (see AssetMount::id)
        Uuid              uuid;         // uuid of the asset
        std::vector<Uuid> dependencies; // dependent assets
    };
    using AssetSourcePtr = CounterPtr<AssetSourceInfo>;

    enum class AssetBuildRetCode : uint32_t {
        SUCCESS,
        FAILED,
    };

    struct AssetImportRequest {
        FilePath filePath;
        FilePath savePath;
        Any      config;
    };

    using ProductBundleKey = std::string;
    struct AssetBuildRequest {
        FilePtr          file;
        AssetSourcePtr   assetInfo;
        ProductBundleKey target; // logical cook target (override/state key)
        ProductBundleKey bundle; // resolved product bundle (preset key); falls back to `target`
        // Resolved per-asset cook override for `target` (sparse: only overridden keys). Applied by the
        // builder over its global preset; empty means "use the global preset".
        std::map<std::string, std::string> settings;
    };

    struct AssetBuildResult {
        Uuid              uuid;
        ProductBundleKey  target;
        AssetBuildRetCode retCode;
        std::string       error;
    };

    struct AssetRawData {
        std::vector<uint8_t> storage;
    };
} // namespace sky
