//
// Created by Zach Lee on 2023/4/7.
//

#pragma once

#include <rhi/DescriptorSet.h>
#include <rhi/GraphicsPipeline.h>
#include <rhi/VertexAssembly.h>
#include <scene_render/Material.h>
#include <scene_render/Technique.h>
#include <core/math/Matrix4.h>

namespace sky::rhi {

    struct LocalData {
        Matrix4 localData = Matrix4::Identity();
    };

    struct SubMesh {
        uint32_t firstIndex;
        uint32_t indexCount;
        MaterialPtr material;
        GFXTechniquePtr tech;
    };

    struct Mesh {
        void SetVA(const VertexAssemblyPtr &va) { vao = va; }
        void AddSubMesh(const SubMesh &subMesh) { subMeshes.emplace_back(subMesh); }
        void SetLocalSet(const DescriptorSetPtr &set);
        void Update();

        LocalData localData;
        VertexAssemblyPtr vao;
        DescriptorSetPtr descriptorSet;
        BufferPtr localBuffer;
        std::vector<SubMesh> subMeshes;
    };
    using MeshPtr = std::shared_ptr<Mesh>;

}