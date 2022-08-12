//
// Created by Zach Lee on 2022/8/12.
//

#include <builders/model/ModelBuilder.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace sky {

    const std::vector<std::string>& ModelBuilder::GetExtensions() const
    {
        static const std::vector<std::string> extensions = {
            ".fbx", ".gltf", ".glb"
        };
        return extensions;
    }

    void ModelBuilder::Build(const std::string& path)
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
        if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            return;
        }
    }

}