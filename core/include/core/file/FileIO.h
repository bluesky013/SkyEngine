//
// Created by Zach Lee on 2022/1/16.
//

#pragma once
#include <string>
#include <vector>

namespace sky {

    bool ReadBin(const std::string& path, std::vector<uint32_t>& out);

    bool ReadString(const std::string& path, std::string& out);

}