//
// Created by blues on 2023/10/11.
//

#include "core/archive/StreamArchive.h"

namespace sky {

    bool IStreamArchive::Load(char *data, uint32_t size)
    {
        return stream.rdbuf()->sgetn(data, size) == size;
    }

    bool IStreamArchive::LoadString(std::string &str)
    {
        uint32_t length = 0;
        if (!Load(length)) {
            return false;
        }
        str.resize(length);
        Load(str.data(), length);
        return true;
    }

    bool OStreamArchive::Save(const char *data, uint32_t size)
    {
        return stream.rdbuf()->sputn(data, size) == size;
    }

    bool OStreamArchive::SaveString(const std::string &str)
    {
        Save(static_cast<uint32_t>(str.size()));
        Save(str.data(), static_cast<uint32_t>(str.size()));
        return true;
    }

} // namespace sky