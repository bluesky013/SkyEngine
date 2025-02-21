//
// Created by Zach Lee on 2023/9/9.
//

#include <render/mesh/MeshRenderer.h>
#include <render/mesh/MeshFeature.h>
#include <render/resource/Meshlet.h>
#include <render/RenderBuiltinLayout.h>
#include <render/Renderer.h>
#include <render/RHI.h>
#include <core/math/MathUtil.h>

namespace sky {

    static constexpr uint32_t MESH_GROUP_SIZE = 32;

    MeshRenderer::~MeshRenderer()
    {
        for (auto &prim : primitives) {
            scene->RemovePrimitive(prim.get());
        }

        if (meshletDebug) {
            scene->RemovePrimitive(meshletDebug->GetPrimitive());
        }
    }

    void MeshRenderer::Tick()
    {
    }

    void MeshRenderer::AttachScene(RenderScene *scn)
    {
        scene = scn;
    }

    void MeshRenderer::SetMaterial(const RDMaterialInstancePtr &mat, uint32_t subMesh)
    {
        auto &primitive = primitives[subMesh];
        primitive->batches.clear();

        const auto &techniques = mat->GetMaterial()->GetGfxTechniques();
        primitive->material = mat;
        primitive->batches.reserve(techniques.size());
        for (const auto &tech : techniques) {
            RenderBatch batch = {tech};
            primitive->batches.emplace_back(batch);
        }
    }

    void MeshRenderer::BuildGeometry()
    {
        enableMeshShading = false;

        uint32_t index = 0;
        auto *meshFeature = MeshFeature::Get();
        for (const auto &sub : mesh->GetSubMeshes()) {
            auto &primitive = primitives.emplace_back(std::make_unique<RenderMaterialPrimitive>());
            primitive->localBound = sub.aabb;
            primitive->geometry   = mesh->GetGeometry();
            primitive->geometry->Upload();
            primitive->clusterValid = enableMeshShading && mesh->ClusterValid();

            primitive->vertexFlags |= (primitive->clusterValid) ? RenderVertexFlagBit::MESH_SHADER : RenderVertexFlagBit::NONE;

            if (primitive->geometry->attributeSemantics.TestBit(VertexSemanticFlagBit::HAS_SKIN)) {
                FillVertexFlags(primitive->vertexFlags);
            }

            const auto &cluster = primitive->geometry->cluster;
            if (cluster && primitive->clusterValid) {
                uint32_t posOffset = sub.firstVertex * sizeof(Vector4);
                uint32_t posSize = sub.vertexCount * sizeof(Vector4);

                uint32_t extOffset = sub.firstVertex * sizeof(Vector4) * 4;
                uint32_t extSize = sub.vertexCount * sizeof(Vector4) * 4;

                meshletInfos[index]->WriteT(0, MeshletInfo{sub.firstMeshlet, sub.meshletCount});

                primitive->instanceSet = meshFeature->RequestMeshResourceGroup();
                primitive->instanceSet->BindDynamicUBO(Name("Local"), ubo, 0);
                primitive->instanceSet->BindDynamicUBO(Name("MeshletInfo"), meshletInfos[index], 0, sizeof(MeshletInfo), 0);
                primitive->instanceSet->BindBuffer(Name("PositionBuf"), cluster->posBuffer->GetRHIBuffer(), posOffset, posSize, 0);
                primitive->instanceSet->BindBuffer(Name("ExtBuf"), cluster->extBuffer->GetRHIBuffer(), extOffset, extSize, 0);

                primitive->instanceSet->BindBuffer(Name("VertexIndices"), cluster->meshletVertices->GetRHIBuffer(), 0);
                primitive->instanceSet->BindBuffer(Name("MeshletTriangles"), cluster->meshletTriangles->GetRHIBuffer(), 0);
                primitive->instanceSet->BindBuffer(Name("Meshlets"), cluster->meshlets->GetRHIBuffer(),0);
                primitive->instanceSet->Update();

                primitive->args.emplace_back(rhi::CmdDispatchMesh {
                    Ceil(sub.meshletCount, MESH_GROUP_SIZE), 1, 1
                });
            } else {
                primitive->instanceSet = meshFeature->RequestResourceGroup();
                primitive->instanceSet->BindDynamicUBO(Name("Local"), ubo, 0);
                primitive->instanceSet->Update();

                primitive->args.emplace_back(rhi::CmdDrawIndexed {
                    sub.indexCount,
                    1,
                    sub.firstIndex,
                    static_cast<int32_t>(sub.firstVertex),
                    0
                });
            }
            scene->AddPrimitive(primitive.get());

            SetMaterial(sub.material, index++);
        }

        for (auto &meshlet : meshletInfos) {
            meshlet->Upload();
        }

        // build meshlet debug render
        if (enableMeshShading) {
            SetupDebugMeshlet();
        }
    }

    void MeshRenderer::SetupDebugMeshlet()
    {
        const auto &geom = mesh->GetGeometry();

        meshletDebug = std::make_unique<MeshletDebugRender>();
        meshletDebug->Setup(geom->cluster->meshlets);

        auto *primitive = meshletDebug->GetPrimitive();
        primitive->instanceSet = MeshFeature::Get()->RequestResourceGroup();
        primitive->instanceSet->BindDynamicUBO(Name("Local"), ubo, 0);
        primitive->instanceSet->Update();
    }

    void MeshRenderer::Reset()
    {
        if (scene == nullptr) {
            return;
        }

        for (auto &prim : primitives) {
            scene->RemovePrimitive(prim.get());
        }
        primitives.clear();
        if (meshletDebug) {
            scene->RemovePrimitive(meshletDebug->GetPrimitive());
            meshletDebug = nullptr;
        }
    }

    void MeshRenderer::SetMesh(const RDMeshPtr &mesh_, bool meshShading)
    {
        mesh = mesh_;
        enableMeshShading = meshShading;

        if (!ubo) {
            PrepareUBO();
        }

        Reset();
        BuildGeometry();
    }

    void MeshRenderer::SetDebugFlags(const MeshDebugFlags& flag)
    {
        debugFlags = flag;
        for (auto &prim : primitives) {
            for (auto &batch : prim->batches) {
                batch.SetOption(Name("MESH_SHADER_DEBUG"), static_cast<uint8_t>(debugFlags.TestBit(MeshDebugFlagBit::MESHLET)));
            }

            for (auto &batch : prim->batches) {
                batch.polygonMode = debugFlags.TestBit(MeshDebugFlagBit::MESH) ? rhi::PolygonMode::LINE : rhi::PolygonMode::FILL;
            }

        }

        if (meshletDebug) {
            scene->RemovePrimitive(meshletDebug->GetPrimitive());
            if (debugFlags.TestBit(MeshDebugFlagBit::MESHLET_CONE)) {
                scene->AddPrimitive(meshletDebug->GetPrimitive());
            }
        }
    }

    void MeshRenderer::UpdateTransform(const Matrix4 &matrix)
    {
        ubo->WriteT(0, matrix);
        ubo->WriteT(sizeof(Matrix4), matrix.InverseTranspose());
        ubo->Upload();

        for (auto &prim : primitives) {
            prim->worldBound = AABB::Transform(prim->localBound, matrix);
        }
    }

    void MeshRenderer::PrepareUBO()
    {
        ubo = new DynamicUniformBuffer();
        ubo->Init(sizeof(InstanceLocal));
        ubo->WriteT(0, InstanceLocal{Matrix4::Identity(), Matrix4::Identity()});
        ubo->Upload();

        meshletInfos.resize(static_cast<uint32_t>(mesh->GetSubMeshes().size()));
        for (auto &meshletInfo : meshletInfos) {
            meshletInfo = new DynamicUniformBuffer();
            meshletInfo->Init(sizeof(MeshletInfo));
            meshletInfo->WriteT(0, MeshletInfo{0, 0});
        }
    }

    RDResourceGroupPtr MeshRenderer::RequestResourceGroup(MeshFeature *feature)
    {
        return feature->RequestResourceGroup();
    }

} // namespace sky
