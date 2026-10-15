//
// Created by blues on 2024/5/16.
//

#pragma once

#include <core/archive/StreamArchive.h>
#include <sstream>

struct AAsset;

namespace sky {

    class AndroidAssetArchive : public IStreamArchive {
    public:
        explicit AndroidAssetArchive(AAsset *asset);
        ~AndroidAssetArchive() override;

        bool IsValid() const;

    private:
        AAsset           *assetFile;
        std::stringstream ss;
        bool              valid = false;
    };

} // namespace sky
