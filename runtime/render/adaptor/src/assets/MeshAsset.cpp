//
// Created by Zach Lee on 2023/2/26.
//

#include <framework/asset/AssetManager.h>
#include <core/profile/Profiler.h>
#include <render/RHI.h>
#include <render/adaptor/assets/MeshAsset.h>
#include <render/adaptor/assets/SkeletonAsset.h>
#include <render/resource/SkeletonMesh.h>

namespace sky {
    uint32_t MeshDataHeader::MakeVersion()
    {
        return 0;
    }

    void MeshAssetData::Load(BinaryInputArchive &archive)
    {
        archive.LoadValue(version);
        archive.LoadValue(skeleton);

        uint32_t size = 0;

        // materials
        archive.LoadValue(size);
        materials.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            archive.LoadValue(reinterpret_cast<char *>(&materials[i]), sizeof(Uuid));
        }

        // subMesh
        archive.LoadValue(size);
        subMeshes.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            archive.LoadValue(reinterpret_cast<char *>(&subMeshes[i]), sizeof(MeshSubSection));
        }

        // sub mapping
        archive.LoadValue(size);
        subMappings.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            uint32_t tmpSize = 0;
            archive.LoadValue(tmpSize);
            subMappings[i].resize(tmpSize);
            archive.LoadValue(reinterpret_cast<char *>(subMappings[i].data()), tmpSize);
        }

        // buffers
        archive.LoadValue(size);
        buffers.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            archive.LoadValue(buffers[i].type);
            archive.LoadValue(buffers[i].offset);
            archive.LoadValue(buffers[i].size);
            archive.LoadValue(buffers[i].stride);
            archive.LoadValue(buffers[i].rate);
        }

        // vertex streams
        archive.LoadValue(size);
        attributes.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            archive.LoadValue(attributes[i].sematic);
            archive.LoadValue(attributes[i].binding);
            archive.LoadValue(attributes[i].offset);
            archive.LoadValue(attributes[i].format);
        }

        archive.LoadValue(indexBuffer);
        archive.LoadValue(indexType);

        // meshlets
        archive.LoadValue(meshlets);
        archive.LoadValue(meshletVertices);
        archive.LoadValue(meshletTriangles);

        // data size
        archive.LoadValue(dataSize);
        dataOffset = static_cast<uint32_t>(archive.GetStream().Tell());
    }

    void MeshAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(version);
        archive.SaveValue(skeleton);

        archive.SaveValue(static_cast<uint32_t>(materials.size()));
        for (const auto &uuid : materials) {
            archive.SaveValue(reinterpret_cast<const char*>(&uuid), sizeof(Uuid));
        }

        // subMesh
        archive.SaveValue(static_cast<uint32_t>(subMeshes.size()));
        for (const auto &subMesh : subMeshes) {
            archive.SaveValue(reinterpret_cast<const char*>(&subMesh), sizeof(MeshSubSection));
        }

        archive.SaveValue(static_cast<uint32_t>(subMappings.size()));
        for (const auto &subMapping : subMappings) {
            archive.SaveValue(static_cast<uint32_t>(subMapping.size()));
            archive.SaveValue(reinterpret_cast<const char*>(subMapping.data()), subMapping.size());
        }

        // primitives
        archive.SaveValue(static_cast<uint32_t>(buffers.size()));
        for (const auto &primitive : buffers) {
            archive.SaveValue(primitive.type);
            archive.SaveValue(primitive.offset);
            archive.SaveValue(primitive.size);
            archive.SaveValue(primitive.stride);
            archive.SaveValue(primitive.rate);
        }

        // vertex streams
        archive.SaveValue(static_cast<uint32_t>(attributes.size()));
        for (const auto &stream : attributes) {
            archive.SaveValue(stream.sematic);
            archive.SaveValue(stream.binding);
            archive.SaveValue(stream.offset);
            archive.SaveValue(stream.format);
        }

        // index
        archive.SaveValue(indexBuffer);
        archive.SaveValue(indexType);

        // meshlets
        archive.SaveValue(meshlets);
        archive.SaveValue(meshletVertices);
        archive.SaveValue(meshletTriangles);

        // data size
        archive.SaveValue(dataSize);

        SKY_ASSERT(dataSize == static_cast<uint32_t>(rawData.storage.size()));

        // save raw
        archive.SaveValue(reinterpret_cast<const char*>(rawData.storage.data()), dataSize);
    }

    struct BoneNode {
        Name name = Name("Root");
        uint32_t boneIndex = INVALID_BONE_ID;
        std::vector<BoneNode*> children;
        Matrix4 globalTransform;
    };

    void WalkBone(BoneNode* node, const Matrix4 &parent, const SkeletonData& skeleton) // NOLINT
    {
        auto globalTrans = parent;
        if (node->boneIndex != INVALID_BONE_ID) {
            auto localTrans = skeleton.refPos[node->boneIndex].ToMatrix();
            globalTrans = parent * localTrans;
            node->globalTransform = globalTrans;
        }

        for (auto &child : node->children) {
            WalkBone(child, globalTrans, skeleton);
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

            if (iter->sematic == VertexSemanticFlagBit::POSITION && view.stride == sizeof(Vector3)) {
                triangleMesh->vtxStride = view.stride;
                triangleMesh->position.swap(rawData);
            } else {
                const uint8_t* rawBuffer = rawData.data();
                triangleMesh->vtxStride = sizeof(Vector3);
                triangleMesh->position.resize(count * sizeof(Vector3));

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

    CounterPtr<Mesh> CreateMeshFromAsset(const MeshAssetPtr &asset, bool buildSkin)
    {
        SKY_PROFILE_NAME("Create Mesh From Asset")
        const auto &data = asset->Data();
        const auto &uuid = asset->GetUuid();

        auto *am = AssetManager::Get();
        auto file = am->OpenFile(uuid);
        SKY_ASSERT(file);

        Mesh* mesh = nullptr;
        if (data.skeleton && buildSkin) {
            mesh = new SkeletonMesh();
        } else {
            mesh = new Mesh();
        }

        std::vector<CounterPtr<MaterialInstance>> materials;
        for (const auto &mat : data.materials) {
            auto matAsset = am->FindAsset<MaterialInstance>(mat);
            materials.emplace_back(CreateMaterialInstanceFromAsset(matAsset));
        }

        for (const auto &sub : data.subMeshes) {
            mesh->AddSubMesh(SubMesh {
                sub.firstVertex,
                sub.vertexCount,
                sub.firstIndex,
                sub.indexCount,
                sub.firstMeshlet,
                sub.meshletCount,
                materials[sub.materialIndex],
                sub.aabb
            });
        }

        mesh->SetVertexAttributes(data.attributes);
        mesh->SetIndexType(data.indexType);

        if (data.skeleton && buildSkin) {
            auto skeleton = am->LoadAsset<Skeleton>(data.skeleton);
            skeleton->BlockUntilLoaded();
            const auto &skeletonData = skeleton->Data();

            auto boneNum = static_cast<uint32_t>(skeletonData.refPos.size());
            BoneNode root;
            std::vector<BoneNode> bones(boneNum);
            for (uint32_t index = 0; index < boneNum; ++index) {
                const auto &boneData = skeletonData.boneData[index];
                bones[index].boneIndex = index;
                bones[index].name = boneData.name;
                if (boneData.parentIndex == INVALID_BONE_ID) {
                    root.children.emplace_back(&bones[index]);
                } else {
                    bones[boneData.parentIndex].children.emplace_back(&bones[index]);
                }
            }
            WalkBone(&root, Matrix4::Identity(), skeletonData);

            auto *sklMesh = static_cast<SkeletonMesh*>(mesh);
            for (uint32_t index = 0; index < mesh->GetSubMeshes().size(); ++index) {
                SkinPtr skin = new Skin();
                skin->boneMapping = data.subMappings[index];
                for (uint32_t i = 0; i < skin->boneMapping.size(); ++i) {
                    skin->boneMatrices[i] = bones[skin->boneMapping[i]].globalTransform.Inverse();
                }
                sklMesh->SetSkin(skin, index);
            }
        }

        MeshUploadData meshData = {};
        auto *fileStream = new rhi::FileStream(file, data.dataOffset);

        for (const auto &attr : data.attributes) {
            if (attr.binding >= meshData.vertexStreams.size()) {
                meshData.vertexStreams.resize(attr.binding + 1);
            }
            const auto &buffer = data.buffers[attr.binding];
            auto& stream = meshData.vertexStreams[attr.binding];
            if (!stream.source.source) {
                stream.source.source = fileStream;
                stream.source.offset = buffer.offset;
                stream.source.size   = buffer.size;
                stream.stride = buffer.stride;
            }
        }

        if (data.indexBuffer != INVALID_MESH_BUFFER_VIEW) {
            SKY_ASSERT(data.indexBuffer < data.buffers.size());
            rhi::BufferUploadRequest &request = meshData.indexStream;
            const auto &buffer = data.buffers[data.indexBuffer];
            request.source = fileStream;
            request.offset = buffer.offset;
            request.size   = buffer.size;
        }

        if (data.meshlets != INVALID_MESH_BUFFER_VIEW) {
            SKY_ASSERT(data.meshlets < data.buffers.size());
            rhi::BufferUploadRequest &request = meshData.meshlets;
            const auto &buffer = data.buffers[data.meshlets];
            request.source = fileStream;
            request.offset = buffer.offset;
            request.size   = buffer.size;
        }

        if (data.meshletVertices != INVALID_MESH_BUFFER_VIEW) {
            SKY_ASSERT(data.meshletVertices < data.buffers.size());
            rhi::BufferUploadRequest &request = meshData.meshletVertices;
            const auto &buffer = data.buffers[data.meshletVertices];
            request.source = fileStream;
            request.offset = buffer.offset;
            request.size   = buffer.size;
        }

        if (data.meshletTriangles != INVALID_MESH_BUFFER_VIEW) {
            SKY_ASSERT(data.meshletTriangles < data.buffers.size());
            rhi::BufferUploadRequest &request = meshData.meshletTriangles;
            const auto &buffer = data.buffers[data.meshletTriangles];
            request.source = fileStream;
            request.offset = buffer.offset;
            request.size   = buffer.size;
        }

        mesh->SetUploadStream(std::move(meshData));
        return mesh;
    }

    MeshAssetData StaticMeshAsset::MakeMeshAssetData() const
    {
        MeshAssetData outData = {};

        outData.version = MeshDataHeader::MakeVersion();
        outData.skeleton = Uuid::GetEmpty();
        outData.materials = materials;
        outData.subMeshes = geometry->GetSubMeshes();

        auto *posBuffer = geometry->GetPositionBuffer();

        auto *uv0Buffer = geometry->GetTexCoordBuffer();
        auto *normalBuffer = geometry->GetNormalBuffer();
        auto *tangentBuffer = geometry->GetTangentBuffer();
        // auto *colorBuffer = geometry->GetColorBuffer();
        auto *indexBuffer = geometry->GetIndexBuffer();

        uint32_t vtxNum = posBuffer->Num();
        uint32_t idxNum = indexBuffer->Num();

        std::vector<Vector3> positions(vtxNum);
        std::vector<VF_TB_UVN<1>> vfBuffer(vtxNum);

        for (uint32_t i = 0; i < vtxNum; ++i) {
            positions[i] = posBuffer->GetVertexData<Vector3>(i);

            vfBuffer[i].normal = normalBuffer->GetVertexData<Vector3>(i);
            vfBuffer[i].tangent = tangentBuffer->GetVertexData<Vector4>(i);
            vfBuffer[i].texCoord[0] = uv0Buffer->GetVertexData<Vector2>(i);
        }

        outData.attributes.emplace_back(VertexAttribute{.sematic=VertexSemanticFlagBit::POSITION, .binding=0, .offset=0, .format=rhi::Format::F_RGB32});

        outData.attributes.emplace_back(VertexAttribute{.sematic=VertexSemanticFlagBit::NORMAL, .binding=1, .offset=OFFSET_OF(VF_TB_UVN<1>, normal), .format=rhi::Format::F_RGB32});
        outData.attributes.emplace_back(VertexAttribute{.sematic=VertexSemanticFlagBit::TANGENT, .binding=1, .offset=OFFSET_OF(VF_TB_UVN<1>, tangent), .format=rhi::Format::F_RGBA32});
        outData.attributes.emplace_back(VertexAttribute{.sematic=VertexSemanticFlagBit::UV, .binding=1, .offset=OFFSET_OF(VF_TB_UVN<1>, texCoord), .format=rhi::Format::F_RG32});

        if (skeletalGeometry) {
            outData.attributes.emplace_back(VertexAttribute{.sematic=VertexSemanticFlagBit::JOINT, .binding=2, .offset=OFFSET_OF(VertexBoneData, boneId), .format=rhi::Format::U_RGBA8});
            outData.attributes.emplace_back(VertexAttribute{.sematic=VertexSemanticFlagBit::WEIGHT, .binding=2, .offset=OFFSET_OF(VertexBoneData, weight), .format=rhi::Format::F_RGBA32});
        }

        auto currentSize = static_cast<uint32_t>(outData.rawData.storage.size());
        auto currentBytes = static_cast<uint32_t>(vtxNum * sizeof(Vector3));
        outData.rawData.storage.resize(currentSize + currentBytes);
        memcpy(outData.rawData.storage.data() + currentSize, positions.data(), currentBytes);
        outData.buffers.emplace_back(MeshBufferView{.offset=currentSize, .size=currentBytes, .stride=sizeof(Vector3)});

        currentSize += currentBytes;
        currentBytes = static_cast<uint32_t>(vtxNum * sizeof(VF_TB_UVN<1>));
        outData.rawData.storage.resize(currentSize + currentBytes);
        memcpy(outData.rawData.storage.data() + currentSize, vfBuffer.data(), currentBytes);
        outData.buffers.emplace_back(MeshBufferView{.offset=currentSize, .size=currentBytes, .stride=sizeof(VF_TB_UVN<1>)});

        if (skeletalGeometry) {
            currentSize += currentBytes;
            auto* boneAndWeight = skeletalGeometry->GetBoneAndWeight();
            currentBytes = boneAndWeight->Num() * boneAndWeight->GetStride();
            outData.rawData.storage.resize(currentSize + currentBytes);
            memcpy(outData.rawData.storage.data() + currentSize, boneAndWeight->GetDataPointer(), currentBytes);
            outData.buffers.emplace_back(MeshBufferView{.offset=currentSize, .size=currentBytes, .stride=sizeof(VertexBoneData)});
        }

        uint32_t indexStride = indexBuffer->GetIndexType() == rhi::IndexType::U32 ? sizeof(uint32_t) : sizeof(uint16_t);
        currentSize += currentBytes;
        currentBytes = idxNum * indexStride;
        outData.rawData.storage.resize(currentSize + currentBytes);
        memcpy(outData.rawData.storage.data() + currentSize, indexBuffer->GetDataPointer(), currentBytes);
        outData.indexType = indexBuffer->GetIndexType();
        outData.indexBuffer = static_cast<uint32_t>(outData.buffers.size());
        outData.buffers.emplace_back(MeshBufferView{.offset = currentSize, .size=currentBytes, .stride=indexStride});

        outData.dataSize = static_cast<uint32_t>(outData.rawData.storage.size());

        return outData;
    }
}
