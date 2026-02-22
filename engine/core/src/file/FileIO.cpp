//
// Created by Zach Lee on 2022/1/16.
//

#include <core/file/FileIO.h>
#include <fstream>
#include <sstream>

namespace sky {

    void WriteBin(const FilePath &path, const char *data, size_t size)
    {
        std::fstream file(path.OpenFStream(std::ios::binary | std::ios::trunc | std::ios::out));
        if (!file.is_open()) {
            return;
        }
        file.write(data, size);
    }

    void WriteString(const FilePath &path, const std::string &out)
    {
        std::fstream file(path.OpenFStream(std::ios::binary | std::ios::trunc | std::ios::out));
        if (!file.is_open()) {
            return;
        }
        file.write(out.data(), out.size() + 1);
    }

    bool ReadBin(const FilePath &path, uint8_t *&out, uint32_t &size)
    {
        std::fstream file(path.OpenFStream(std::ios::binary | std::ios::ate | std::ios::in));
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

    BinaryDataPtr ReadBin(const FilePath &path)
    {
        std::fstream file(path.OpenFStream(std::ios::binary | std::ios::ate | std::ios::in));
        if (!file.is_open()) {
            return nullptr;
        }
        auto fileSize = static_cast<uint32_t>(file.tellg());
        BinaryDataPtr data = new BinaryData(fileSize);

        file.seekg(0);
        file.read(reinterpret_cast<char *>(data->Data()), fileSize);
        file.close();
        return data;
    }

    bool ReadBin(const FilePath &path, std::vector<uint8_t> &out)
    {
        std::fstream file(path.OpenFStream(std::ios::binary | std::ios::ate | std::ios::in));
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

    bool ReadBin(const FilePath &path, std::vector<uint32_t> &out)
    {
        std::fstream file(path.OpenFStream(std::ios::binary | std::ios::ate | std::ios::in));
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

    bool ReadString(const FilePath &path, std::string &out)
    {
        std::fstream file(path.OpenFStream(std::ios::binary | std::ios::in));
        if (!file.is_open()) {
            return false;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        out = buffer.str();
        return true;
    }

} // namespace sky