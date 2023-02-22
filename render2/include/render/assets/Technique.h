//
// Created by Zach Lee on 2023/2/23.
//

#pragma once

namespace sky {
    class BinaryInputArchive;
    class BinaryOutputArchive;

    struct TechniqueAssetData {
        void Load(BinaryInputArchive &archive) {}
        void Save(BinaryOutputArchive &archive) const {}
    };

    class Technique {
        Technique() = default;
        ~Technique() = default;
    };
}