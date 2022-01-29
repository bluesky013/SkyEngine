//
// Created by Zach Lee on 2022/1/27.
//

#pragma once
#include <string>
#include <vector>
#include <engine/asset/MeshAsset.h>

struct aiScene;

namespace sky {

    class ModelLoader {
    public:
        ModelLoader() = default;
        ~ModelLoader() = default;

        bool Load(const std::string& path);

        void LoadMesh();

        void LoadTextures();

        void LoadMaterial();

        void Save(const std::string& path);

    private:
        const aiScene* scene = nullptr;

        MeshData model;
    };

}