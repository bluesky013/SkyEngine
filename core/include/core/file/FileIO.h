//
// Created by Zach Lee on 2022/1/16.
//

#pragma once
#include <string>
#include <vector>

namespace sky {

    void WriteBin(const std::string &path, const char *data, size_t size);

    bool ReadBin(const std::string &path, std::vector<uint8_t> &out);

    bool ReadBin(const std::string &path, std::vector<uint32_t> &out);

    bool ReadString(const std::string &path, std::string &out);

} // namespace sky