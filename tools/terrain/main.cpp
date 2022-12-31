//
// Created by Zach Lee on 2022/12/30.
//

#include <iostream>
#include <filesystem>
#include <cxxopts.hpp>
#include <PerlinNoise.hpp>

#include "vulkan/vulkan_core.h"

#define KHRONOS_STATIC
#include "ktx.h"

void Save(const char* path, uint32_t width, const void* data, uint64_t size, uint32_t version) {

    ktxTextureCreateInfo createInfo = {};
    createInfo.baseWidth = width;
    createInfo.baseHeight = width;
    createInfo.baseDepth = 1;
    createInfo.numDimensions = 2;
    createInfo.numLevels = 1;
    createInfo.numLayers = 1;
    createInfo.numFaces = 1;
    createInfo.isArray = KTX_FALSE;
    createInfo.generateMipmaps = KTX_TRUE;

    ktxTexture *result;
    if (version == 1) {
        ktxTexture1 *texture = nullptr;
        createInfo.glInternalformat = 0x822A;
        ktxTexture1_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture);
        result = ktxTexture(texture);
    } else {
        ktxTexture2 *texture = nullptr;
        createInfo.vkFormat = VK_FORMAT_R16_UNORM;
        ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture);
        result = ktxTexture(texture);
    }

    ktxTexture_SetImageFromMemory(result, 0, 0, 0, reinterpret_cast<const ktx_uint8_t*>(data), size);
    ktxTexture_WriteToNamedFile(result, path);
    ktxTexture_Destroy(result);
}

int main(int argc, char *argv[])
{
    cxxopts::Options options("AssetBuilder", "SkyEngine AssetProcessor");
    options.add_options()
        ("p,project", "Project Directory", cxxopts::value<std::string>())
        ("b,block", "Block Number, default 1", cxxopts::value<uint32_t>())
        ("w,width", "Block Width, default 1024", cxxopts::value<uint32_t>())
        ("v,version", "ktx version, default 1", cxxopts::value<uint32_t>())
        ("h,help", "Print usage");

    auto result = options.parse(argc, argv);
    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        exit(0);
    }

    std::string projectPath = "";
    if (result.count("project")) {
        projectPath = result["project"].as<std::string>();
    }

    uint32_t version = 1;
    uint32_t blockNum = 1;
    uint32_t blockWidth = 1024;

    if (result.count("block")) {
        blockNum = result["block"].as<uint32_t>();
    }

    if (result.count("width")) {
        blockWidth = result["width"].as<uint32_t>();
    }

    if (result.count("version")) {
        version = result["version"].as<uint32_t>();
    }

    std::cout << "Project Path: " << projectPath << std::endl;

    const siv::PerlinNoise::seed_type seed = 123456u;
    const siv::PerlinNoise perlin{ seed };

    auto fn = [&](uint32_t x, uint32_t y) {
        std::stringstream ss;
        ss << projectPath << "terrain_" << x << "_" << y << ".ktx";
        std::vector<uint16_t> data(blockWidth * blockWidth, 0);

        uint64_t xOffset = x * blockWidth;
        uint64_t yOffset = y * blockWidth;

        for (uint64_t i = 0; i < blockWidth; ++i) {
            for (uint64_t j = 0; j < blockWidth; ++j) {
                const double noise       = perlin.noise2D_01((xOffset + i) * 0.002, (yOffset + j) * 0.002);
                data[i * blockWidth + j] = noise * 65535;
            }
        }

        Save(ss.str().c_str(), blockWidth, data.data(), data.size() * sizeof(uint16_t), version);
    };

    for (uint32_t i = 0; i < blockNum; ++i) {
        for (uint32_t j = 0; j < blockNum; ++j) {
            fn(i, j);
        }
    }

    return 0;
}
