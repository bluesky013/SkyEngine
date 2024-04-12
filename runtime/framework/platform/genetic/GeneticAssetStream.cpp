//
// Created by bluesky on 2023/4/15.
//

#include <framework/asset/AssetStream.h>
#include <core/file/FileIO.h>

namespace sky {

    std::string AssetStream::ReadString() {
        std::string res;
        sky::ReadString(path, res);
        return res;
    }
} // namespace sky