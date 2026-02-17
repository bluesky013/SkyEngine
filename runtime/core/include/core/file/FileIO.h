//
// Created by Zach Lee on 2022/1/16.
//

#pragma once
#include <string>
#include <vector>
#include <core/file/FileSystem.h>
#include <core/archive/BinaryData.h>

namespace sky {

    void WriteBin(const FilePath &path, const char *data, size_t size);
    void WriteString(const FilePath &path, const std::string &out);

    bool ReadBin(const FilePath &path, uint8_t *&out, uint32_t &size);
    bool ReadBin(const FilePath &path, std::vector<uint8_t> &out);
    bool ReadBin(const FilePath &path, std::vector<uint32_t> &out);
    BinaryDataPtr ReadBin(const FilePath &path);

    bool ReadString(const FilePath &path, std::string &out);

} // namespace sky