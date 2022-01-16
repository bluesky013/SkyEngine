//
// Created by Zach Lee on 2022/1/16.
//

#include <core/file/FileIO.h>
#include <fstream>

namespace sky {

    bool ReadBin(const std::string& path, std::vector<uint32_t>& out)
    {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        auto fileSize = (uint32_t)file.tellg();
        out.resize(fileSize / sizeof(uint32_t));
        file.seekg(0);
        file.read((char*)out.data(), fileSize);
        file.close();
        return true;
    }

    bool ReadString(const std::string& path, std::string& out)
    {
        std::ifstream file(path, std::ios::ate);
        if (!file.is_open()) {
            return false;
        }
        auto fileSize = (uint32_t)file.tellg();
        out.resize(fileSize + 1);
        file.seekg(0);
        file.read((char*)out.data(), fileSize);
        out[fileSize] = '\0';
        file.close();
        return true;
    }

}