//
// Created by Zach Lee on 2023/4/7.
//

#pragma once

#include <rhi/DescriptorSet.h>
#include <scene_render/Mesh.h>
#include <scene_render/Camera.h>

namespace sky::rhi {

    class Scene {
    public:
        Scene() = default;
        ~Scene() = default;

        void SetGlobalSet(const DescriptorSetPtr &set) { globalSet = set; }
        const DescriptorSetPtr &GetGlobalSet() const { return globalSet; }

        void AddMesh(const MeshPtr &mesh) { meshes.emplace_back(mesh); }
        void AddCamera(const CameraPtr &cam) { cameras.emplace_back(cam); }

        void Tick() {}

    private:
        DescriptorSetPtr globalSet;

        std::vector<MeshPtr> meshes;
        std::vector<CameraPtr> cameras;
    };

}