//
// Created by Zach Lee on 2022/1/16.
//

#include <core/file/FileIO.h>
#include <fstream>
#include <sstream>

namespace sky {

    void WriteBin(const std::string &path, const char *data, size_t size)
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            return;
        }
        file.write(data, size);
    }

    void WriteString(const std::string &path, const std::string &out)
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            return;
        }
        file.write(out.data(), out.size() + 1);
    }

    bool ReadBin(const std::string &path, uint8_t *&out, uint32_t &size)
    {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        size = (uint32_t)file.tellg();
        out = new uint8_t[size];
        file.seekg(0);
        file.read((char *)out, size);
        file.close();
        return true;
    }

    bool ReadBin(const std::string &path, std::vector<uint8_t> &out)
    {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        auto fileSize = (uint32_t)file.tellg();
        out.resize(fileSize);
        file.seekg(0);
        file.read((char *)out.data(), fileSize);
        file.close();
        return true;
    }

    bool ReadBin(const std::string &path, std::vector<uint32_t> &out)
    {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        auto fileSize = (uint32_t)file.tellg();
        out.resize(fileSize / sizeof(uint32_t));
        file.seekg(0);
        file.read((char *)out.data(), fileSize);
        file.close();
        return true;
    }

    bool ReadString(const std::string &path, std::string &out)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        out = buffer.str();
        return true;
    }

} // namespace sky