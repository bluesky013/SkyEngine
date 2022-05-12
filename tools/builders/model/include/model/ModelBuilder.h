//
// Created by Zach Lee on 2022/1/27.
//

#pragma once
#include <string>
#include <vector>

struct aiScene;

namespace sky {

    class ModelBuilder {
    public:
        ModelBuilder();
        ~ModelBuilder();

        bool Load(const std::string& path);

        void LoadMesh();

        void LoadTextures();

        void LoadMaterial();

        void Save(const std::string& path);

    private:
        const aiScene* scene = nullptr;
    };

}