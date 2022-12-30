//
// Created by Zach Lee on 2022/12/30.
//

#include <iostream>
#include <filesystem>
#include <cxxopts.hpp>
#include <PerlinNoise.hpp>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

int main(int argc, char *argv[])
{
    cxxopts::Options options("AssetBuilder", "SkyEngine AssetProcessor");
    options.add_options()
        ("p,project", "Project Directory", cxxopts::value<std::string>())
        ("b,block", "Block Number", cxxopts::value<uint32_t>())
        ("w,width", "Block Width", cxxopts::value<uint32_t>())
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

    uint32_t blockNum = 1;
    uint32_t blockWidth = 1024;
    if (result.count("block")) {
        blockNum = result["block"].as<uint32_t>();
    }

    if (result.count("width")) {
        blockWidth = result["width"].as<uint32_t>();
    }
    std::cout << "Project Path: " << projectPath << std::endl;

    const siv::PerlinNoise::seed_type seed = 123456u;
    const siv::PerlinNoise perlin{ seed };

    auto fn = [&](uint32_t x, uint32_t y) {
        std::stringstream ss;
        ss << projectPath << "_" << x << "_" << y << ".jpg";
        std::vector<uint8_t> data(blockWidth * blockWidth, 0);

        uint64_t xOffset = x * blockWidth;
        uint64_t yOffset = y * blockWidth;

        for (uint64_t i = 0; i < blockWidth; ++i) {
            for (uint64_t j = 0; j < blockWidth; ++j) {
                const double noise = perlin.noise2D_01((xOffset + i) * 0.05, (yOffset + j) * 0.05);
                data[i * blockWidth + j] = noise * 255;
            }
        }
        stbi_write_png(ss.str().c_str(), blockWidth, blockWidth, 1, data.data(), blockWidth);
    };

    for (uint32_t i = 0; i < blockNum; ++i) {
        for (uint32_t j = 0; j < blockNum; ++j) {
            fn(i, j);
        }
    }

    return 0;
}
