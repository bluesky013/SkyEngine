//
// Created by Zach Lee on 2023/2/26.
//

#include <framework/asset/AssetManager.h>
#include <core/archive/MemoryArchive.h>
#include <render/RHI.h>
#include <render/adaptor/assets/MeshAsset.h>
#include <render/adaptor/assets/SkeletonAsset.h>
#include <render/resource/SkeletonMesh.h>

namespace sky {
    void MeshAssetData::Load(BinaryInputArchive &archive)
    {
        archive.LoadValue(version);
        archive.LoadValue(skeleton);

        uint32_t size = 0;
        archive.LoadValue(size);

        // subMesh
        subMeshes.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            archive.LoadValue(reinterpret_cast<char *>(&subMeshes[i]), sizeof(SubMeshAssetData));
        }

        // buffers
        archive.LoadValue(size);
        buffers.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            archive.LoadValue(buffers[i].type);
            archive.LoadValue(buffers[i].offset);
            archive.LoadValue(buffers[i].size);
            archive.LoadValue(buffers[i].stride);
        }

        // vertex streams
        archive.LoadValue(size);
        attributes.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            archive.LoadValue(attributes[i].sematic);
            archive.LoadValue(attributes[i].binding);
            archive.LoadValue(attributes[i].offset);
            archive.LoadValue(attributes[i].format);
            archive.LoadValue(attributes[i].rate);
        }

        // data size
        archive.LoadValue(indexBuffer);
        archive.LoadValue(indexType);
        archive.LoadValue(dataSize);
        dataOffset = static_cast<uint32_t>(archive.GetStream().Tell());
    }

    void MeshAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(version);
        archive.SaveValue(skeleton);

        // subMesh
        archive.SaveValue(static_cast<uint32_t>(subMeshes.size()));
        for (const auto &subMesh : subMeshes) {
            archive.SaveValue(reinterpret_cast<const char*>(&subMesh), sizeof(SubMeshAssetData));
        }

        // primitives
        archive.SaveValue(static_cast<uint32_t>(buffers.size()));
        for (const auto &primitive : buffers) {
            archive.SaveValue(primitive.type);
            archive.SaveValue(primitive.offset);
            archive.SaveValue(primitive.size);
            archive.SaveValue(primitive.stride);
        }

        // vertex streams
        archive.SaveValue(static_cast<uint32_t>(attributes.size()));
        for (const auto &stream : attributes) {
            archive.SaveValue(stream.sematic);
            archive.SaveValue(stream.binding);
            archive.SaveValue(stream.offset);
            archive.SaveValue(stream.format);
            archive.SaveValue(stream.rate);
        }

        // index
        archive.SaveValue(indexBuffer);
        archive.SaveValue(indexType);

        // data size
        archive.SaveValue(dataSize);

        SKY_ASSERT(dataSize == static_cast<uint32_t>(rawData.storage.size()));

        // save raw
        archive.SaveValue(reinterpret_cast<const char*>(rawData.storage.data()), dataSize);
    }

    struct BoneNode {
        std::string_view name = "Root";
        uint32_t boneIndex = INVALID_BONE_ID;
        std::vector<BoneNode*> children;
    };

    void WalkBone(BoneNode* node, const Matrix4 &matrix, const SkeletonData& skeleton, Skin &skin) // NOLINT
    {
        Matrix4 current = matrix;
        if (node->boneIndex != INVALID_BONE_ID) {
            current = current * skeleton.refPos[node->boneIndex].ToMatrix();
            skin.boneMatrices[node->boneIndex] = current * skeleton.inverseBindMatrix[node->boneIndex];
        }
        for (auto &child : node->children) {
            WalkBone(child, current, skeleton, skin);
        }
    }

    CounterPtr<TriangleMesh> CreateTriangleMesh(const MeshAssetPtr &asset)
    {
        const auto &data = asset->Data();
        const auto &uuid = asset->GetUuid();

        auto *am = AssetManager::Get();
        auto file = am->OpenFile(uuid);

        auto *triangleMesh = new TriangleMesh();
        // get position.
        auto iter = std::find_if(data.attributes.begin(), data.attributes.end(),
            [](const VertexAttribute &attr) -> bool  {
            return (attr.sematic & VertexSemanticFlagBit::POSITION) == VertexSemanticFlagBit::POSITION;
        });
        SKY_ASSERT(iter != data.attributes.end());
        SKY_ASSERT(iter->format == rhi::Format::F_RGBA32 || iter->format == rhi::Format::F_RGB32);
        SKY_ASSERT(data.indexType == rhi::IndexType::U32 || data.indexType == rhi::IndexType::U16);
        SKY_ASSERT(data.indexBuffer <= data.buffers.size());

        // vertex buffer
        {
            const auto &view = data.buffers[iter->binding];
            const auto &count = view.size / view.stride;

            std::vector<uint8_t> rawData(view.size);
            file->ReadData(view.offset + data.dataOffset, view.size, rawData.data());

            if (iter->sematic == VertexSemanticFlagBit::POSITION) {
                triangleMesh->vtxStride = view.stride;
                triangleMesh->position.swap(rawData);
            } else {
                const uint8_t* rawBuffer = rawData.data();
                triangleMesh->vtxStride = sizeof(Vector3);
                triangleMesh->position.resize(count * 3);

                auto* target = reinterpret_cast<Vector3*>(triangleMesh->position.data());
                for (uint32_t i = 0; i < count; ++i) {
                    const auto* fp = reinterpret_cast<const Vector3*>(rawBuffer);
                    target[i] = *fp;
                    rawBuffer += view.stride;
                }
            }
        }

        // index buffer
        {
            const auto &view = data.buffers[data.indexBuffer];
            triangleMesh->indexType = data.indexType == rhi::IndexType::U32 ? IndexType::U32 : IndexType::U16;
            triangleMesh->indexRaw.resize(view.size);
            file->ReadData(view.offset + data.dataOffset, view.size, triangleMesh->indexRaw.data());
        }

        for (const auto &sub : data.subMeshes) {
            triangleMesh->AddView(sub.firstVertex, sub.vertexCount, sub.firstIndex, sub.indexCount, sub.aabb);
        }

        return triangleMesh;
    }

    CounterPtr<Mesh> CreateMeshFromAsset(const MeshAssetPtr &asset)
    {
        const auto &data = asset->Data();
        const auto &uuid = asset->GetUuid();

        auto *am = AssetManager::Get();
        auto file = am->OpenFile(uuid);
        SKY_ASSERT(file);

        Mesh* mesh = nullptr;
        if (data.skeleton) {
            auto skeleton = am->LoadAsset<Skeleton>(data.skeleton);
            skeleton->BlockUntilLoaded();
            const auto &skeletonData = skeleton->Data();
            auto* skin = new Skin();
            skin->activeBone = static_cast<uint32_t>(skeletonData.refPos.size());
            SKY_ASSERT(skeletonData.boneData.size() == skeletonData.inverseBindMatrix.size());

            BoneNode root;
            std::vector<BoneNode> bones(skin->activeBone);
            for (uint32_t index = 0; index < skin->activeBone; ++index) {
                const auto &boneData = skeletonData.boneData[index];
                bones[index].boneIndex = index;
                bones[index].name = boneData.name;
                if (boneData.parentIndex == INVALID_BONE_ID) {
                    root.children.emplace_back(&bones[index]);
                } else {
                    bones[boneData.parentIndex].children.emplace_back(&bones[index]);
                }
            }
            WalkBone(&root, Matrix4::Identity(), skeletonData, *skin);

            auto *skeletonMesh = new SkeletonMesh();
            skeletonMesh->SetSkin(skin);
            mesh = skeletonMesh;
        } else {
            mesh = new Mesh();
        }

        for (const auto &sub : data.subMeshes) {
            auto matAsset = am->FindAsset<MaterialInstance>(sub.material);
            auto mat = CreateMaterialInstanceFromAsset(matAsset);

            mesh->AddSubMesh(SubMesh {
                sub.firstVertex,
                sub.vertexCount,
                sub.firstIndex,
                sub.indexCount,
                mat,
                sub.aabb
            });
        }

        mesh->SetVertexAttributes(data.attributes);
        mesh->SetIndexType(data.indexType);

        MeshData meshData = {};
        auto *fileStream = new rhi::FileStream(file, data.dataOffset);

        for (const auto &buffer : data.buffers) {
            rhi::BufferUploadRequest request = {};

            request.source = fileStream;
            request.offset = buffer.offset;
            request.size   = buffer.size;
            meshData.vertexStreams.emplace_back(request, buffer.stride);
        }

        if (data.indexType != rhi::IndexType::NONE) {
            SKY_ASSERT(data.indexBuffer < data.buffers.size());
            rhi::BufferUploadRequest &request = meshData.indexStream;
            const auto &buffer = data.buffers[data.indexBuffer];
            request.source = fileStream;
            request.offset = buffer.offset;
            request.size   = buffer.size;
        }
        mesh->SetUploadStream(std::move(meshData));
        return mesh;
    }

}