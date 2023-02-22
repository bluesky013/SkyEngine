//
// Created by Zach Lee on 2023/2/23.
//

#pragma once

namespace sky {
    class BinaryInputArchive;
    class BinaryOutputArchive;

    struct MaterialAssetData {
        void Load(BinaryInputArchive &archive) {}
        void Save(BinaryOutputArchive &archive) const {}
    };

    struct MaterialInstanceAssetData {
        void Load(BinaryInputArchive &archive) {}
        void Save(BinaryOutputArchive &archive) const {}
    };

    class Material {
    public:
        Material() = default;
        ~Material() = default;
    };

    class MaterialInstance {
        MaterialInstance() = default;
        ~MaterialInstance() = default;
    };
}