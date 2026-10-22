//
// Created by blues on 2026/9/29.
//
#include <builder/render/UsdBuilder.h>

#include <core/logger/Logger.h>
#include <core/math/MathUtil.h>

#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/serialization/BinaryArchive.h>
#include <framework/serialization/JsonArchive.h>
#include <render/adaptor/assets/MaterialAsset.h>
#include <render/adaptor/assets/MeshAsset.h>
#include <render/adaptor/assets/LodGroupAsset.h>

#ifdef SKY_WITH_USD
#include <pxr/base/plug/registry.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/subset.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/shader.h>
#include <pxr/usd/sdf/assetPath.h>
#endif

#include <sstream>

static const char* TAG = "UsdBuilder";

#ifdef SKY_WITH_USD
namespace sky::builder {

    // ArDefaultResolver / SDF 格式插件依赖 plugInfo.json；
    // Plug 的默认搜索路径在 dylib 加载时（ARCH_CONSTRUCTOR）就固定了，
    // 运行时 setenv 来不及，这里用 PlugRegistry::RegisterPlugins 显式注册
    static void SetupUsdPluginPath()
    {
        static bool initialized = false;
        if (initialized) {
            return;
        }
        initialized = true;

        auto engineFs = AssetBuilderManager::Get()->GetEngineFs();
        if (engineFs == nullptr) {
            return;
        }
        auto &registry = pxr::PlugRegistry::GetInstance();
        registry.RegisterPlugins((engineFs->GetPath() / FilePath("3rd/output/usd/lib/Release/usd")).GetStr());
        registry.RegisterPlugins((engineFs->GetPath() / FilePath("3rd/output/usd/plugin/usd")).GetStr());
    }


#define OFFSET_OF(s, m) static_cast<uint32_t>(offsetof(s, m))

    // 与 PrefabBuilder 的 StandardVertexData 布局保持一致
    struct StandardVertexData {
        Vector4 uv;
        Vector4 normal;
        Vector4 tangent;
        Vector4 color;
    };

    struct UsdBuildContext {
        std::string name;
        AssetSourcePath path;

        std::unordered_map<std::string, Uuid> textures;  // 相对路径 -> texture uuid
        std::unordered_map<std::string, Uuid> materials; // material prim path -> mati uuid
        std::vector<AssetSourcePtr> meshes;
        std::vector<RenderPrefabNode> nodes;

        Uuid defaultMaterial;
    };

    static inline Matrix4 FromUsd(const pxr::GfMatrix4d &trans)
    {
        // GfMatrix4d 为行主序、行向量约定（平移在第 3 行），与引擎 Matrix4 布局一致，直接拷贝
        Matrix4 res;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                res.v[i * 4 + j] = static_cast<float>(trans[i][j]);
            }
        }
        return res;
    }

    static std::string StripDotSlash(std::string path)
    {
        while (path.rfind("./", 0) == 0) {
            path = path.substr(2);
        }
        return path;
    }

    static Uuid ProcessUsdTexture(const pxr::SdfAssetPath &assetPath, UsdBuildContext &context, const AssetImportRequest &request)
    {
        std::string texRel = StripDotSlash(assetPath.GetAssetPath());
        if (texRel.empty()) {
            return {};
        }

        auto iter = context.textures.find(texRel);
        if (iter != context.textures.end()) {
            return iter->second;
        }

        auto texPath = request.filePath.Parent() / FilePath(texRel);
        if (!texPath.Exist()) {
            LOG_W(TAG, "usd texture not found %s", texPath.GetStr().c_str());
            return {};
        }

        AssetSourcePath sourcePath = {};
        sourcePath.bundle = SourceAssetBundle::WORKSPACE;
        sourcePath.path = context.path.path / FilePath(texRel);
        AssetDataBase::Get()->GetWorkSpaceFs()->Copy(texPath, sourcePath.path);

        auto source = AssetDataBase::Get()->RegisterAsset(sourcePath);
        context.textures.emplace(texRel, source->uuid);
        return source->uuid;
    }

    template <typename T>
    static bool GetShaderInputValue(const pxr::UsdShadeShader &shader, const pxr::TfToken &name, T &out)
    {
        auto input = shader.GetInput(name);
        if (!input) {
            return false;
        }
        return input.Get(&out);
    }

    // 沿 connection 找到上游的 UsdUVTexture shader
    static pxr::UsdShadeShader GetConnectedTextureShader(const pxr::UsdShadeShader &shader, const pxr::TfToken &name)
    {
        auto input = shader.GetInput(name);
        if (!input) {
            return pxr::UsdShadeShader();
        }

        pxr::UsdShadeConnectableAPI source;
        pxr::TfToken sourceName;
        pxr::UsdShadeAttributeType sourceType;
        if (!input.GetConnectedSource(&source, &sourceName, &sourceType)) {
            return pxr::UsdShadeShader();
        }

        pxr::UsdShadeShader sourceShader(source.GetPrim());
        if (!sourceShader) {
            return pxr::UsdShadeShader();
        }

        pxr::TfToken id;
        sourceShader.GetIdAttr().Get(&id);
        if (id != pxr::TfToken("UsdUVTexture")) {
            return pxr::UsdShadeShader();
        }
        return sourceShader;
    }

    static Uuid ProcessUsdTextureInput(const pxr::UsdShadeShader &shader, const pxr::TfToken &name,
                                       UsdBuildContext &context, const AssetImportRequest &request)
    {
        auto texShader = GetConnectedTextureShader(shader, name);
        if (!texShader) {
            return {};
        }

        pxr::SdfAssetPath assetPath;
        if (!GetShaderInputValue(texShader, pxr::TfToken("file"), assetPath)) {
            return {};
        }
        return ProcessUsdTexture(assetPath, context, request);
    }

    static void FillDefaultPbrProperties(MaterialInstanceData &data, const Vector4 &baseColor, float metallic, float roughness,
                                         const Uuid &baseColorMap, const Uuid &mrMap, const Uuid &normalMap)
    {
        uint8_t useBaseColorMap = static_cast<bool>(baseColorMap);
        uint8_t useMetallicRoughnessMap = static_cast<bool>(mrMap);
        uint8_t useNormalMap = static_cast<bool>(normalMap);

        data.properties.valueMap.emplace("ENABLE_AO_MAP", uint8_t(0));
        data.properties.valueMap.emplace("ENABLE_EMISSIVE_MAP", uint8_t(0));

        if (useBaseColorMap) {
            data.properties.valueMap.emplace("AlbedoMap", MaterialTexture{baseColorMap});
        }

        data.properties.valueMap.emplace("ENABLE_MR_MAP", useMetallicRoughnessMap);
        if (useMetallicRoughnessMap) {
            data.properties.valueMap.emplace("MetallicRoughnessMap", MaterialTexture{mrMap});
        }

        data.properties.valueMap.emplace("ENABLE_NORMAL_MAP", useNormalMap);
        if (useNormalMap) {
            data.properties.valueMap.emplace("NormalMap", MaterialTexture{normalMap});
        }

        data.properties.valueMap.emplace("ENABLE_ALPHA_MASK", uint8_t(0));

        data.properties.valueMap.emplace("Albedo", baseColor);
        data.properties.valueMap.emplace("Metallic", metallic);
        data.properties.valueMap.emplace("Roughness", roughness);
        data.properties.valueMap.emplace("AlphaCutoff", 0.5f);
    }

    static Uuid WriteMaterialInstance(const std::string &matName, MaterialInstanceData &data, UsdBuildContext &context)
    {
        AssetSourcePath sourcePath = {};
        sourcePath.bundle = SourceAssetBundle::WORKSPACE;
        sourcePath.path = context.path.path / FilePath(matName);

        {
            auto file = AssetDataBase::Get()->CreateOrOpenFile(sourcePath);
            auto archive = file->WriteAsArchive();
            JsonOutputArchive bin(*archive);
            data.SaveJson(bin);
        }

        auto source = AssetDataBase::Get()->RegisterAsset(sourcePath);
        return source->uuid;
    }

    static Uuid GetDefaultMaterial(UsdBuildContext &context)
    {
        if (context.defaultMaterial) {
            return context.defaultMaterial;
        }

        MaterialInstanceData data = {};
        data.material = AssetDataBase::Get()->RegisterAsset("materials/standard_pbr.mat")->uuid;
        FillDefaultPbrProperties(data, Vector4(1.f, 1.f, 1.f, 1.f), 0.f, 0.5f, {}, {}, {});

        context.defaultMaterial = WriteMaterialInstance(context.name + "_default.mati", data, context);
        return context.defaultMaterial;
    }

    static Uuid ProcessUsdMaterial(const pxr::UsdShadeMaterial &material, UsdBuildContext &context, const AssetImportRequest &request)
    {
        std::string matKey = material.GetPath().GetString();
        auto iter = context.materials.find(matKey);
        if (iter != context.materials.end()) {
            return iter->second;
        }

        MaterialInstanceData data = {};
        data.material = AssetDataBase::Get()->RegisterAsset("materials/standard_pbr.mat")->uuid;

        Vector4 baseColor = Vector4(1.f, 1.f, 1.f, 1.f);
        float metallic = 0.f;
        float roughness = 0.5f;
        Uuid baseColorMap;
        Uuid metallicRoughnessMap;
        Uuid normalMap;

        auto surfaceShader = material.ComputeSurfaceSource();
        if (surfaceShader) {
            pxr::GfVec3f diffuse;
            if (GetShaderInputValue(surfaceShader, pxr::TfToken("diffuseColor"), diffuse)) {
                baseColor = Vector4(diffuse[0], diffuse[1], diffuse[2], 1.f);
            }
            GetShaderInputValue(surfaceShader, pxr::TfToken("metallic"), metallic);
            GetShaderInputValue(surfaceShader, pxr::TfToken("roughness"), roughness);

            baseColorMap = ProcessUsdTextureInput(surfaceShader, pxr::TfToken("diffuseColor"), context, request);
            normalMap = ProcessUsdTextureInput(surfaceShader, pxr::TfToken("normal"), context, request);

            // USD 里 metallic/roughness 是两张独立贴图，standard_pbr 期望打包的 MR 贴图，
            // 这里优先取 roughness 贴图作为近似（灰度图 G 通道即 roughness）
            metallicRoughnessMap = ProcessUsdTextureInput(surfaceShader, pxr::TfToken("roughness"), context, request);
            if (!metallicRoughnessMap) {
                metallicRoughnessMap = ProcessUsdTextureInput(surfaceShader, pxr::TfToken("metallic"), context, request);
            }
        }

        FillDefaultPbrProperties(data, baseColor, metallic, roughness, baseColorMap, metallicRoughnessMap, normalMap);

        std::string matName = material.GetPrim().GetName().GetString() + ".mati";
        Uuid uuid = WriteMaterialInstance(matName, data, context);
        context.materials.emplace(matKey, uuid);
        return uuid;
    }

    // 一个 GeomSubset(materialBind) 或 mesh 整体对应一个 engine submesh
    struct UsdSubMeshGroup {
        pxr::UsdShadeMaterial material;
        std::vector<int> faces;
    };

    static std::vector<UsdSubMeshGroup> CollectSubMeshGroups(const pxr::UsdGeomMesh &mesh)
    {
        size_t faceCount = 0;
        {
            pxr::VtArray<int> counts;
            mesh.GetFaceVertexCountsAttr().Get(&counts);
            faceCount = counts.size();
        }

        // GeomSubset 的子材质划分（familyName = materialBind）
        std::vector<UsdSubMeshGroup> subsets;
        std::vector<uint8_t> assigned(faceCount, 0);
        for (const auto &child : mesh.GetPrim().GetChildren()) {
            if (!child.IsA<pxr::UsdGeomSubset>()) {
                continue;
            }
            pxr::UsdGeomSubset subset(child);
            pxr::TfToken familyName;
            subset.GetFamilyNameAttr().Get(&familyName);
            if (familyName != pxr::TfToken("materialBind")) {
                continue;
            }

            pxr::VtArray<int> subsetFaces;
            subset.GetIndicesAttr().Get(&subsetFaces);

            auto &group = subsets.emplace_back();
            group.material = pxr::UsdShadeMaterialBindingAPI(child).ComputeBoundMaterial();
            for (int f : subsetFaces) {
                if (f >= 0 && static_cast<size_t>(f) < faceCount && !assigned[f]) {
                    assigned[f] = 1;
                    group.faces.emplace_back(f);
                }
            }
        }

        // 未被子集覆盖的面归到 mesh 自身绑定的材质
        pxr::UsdShadeMaterial restMaterial = pxr::UsdShadeMaterialBindingAPI(mesh.GetPrim()).ComputeBoundMaterial();
        std::vector<int> restFaces;
        for (size_t f = 0; f < faceCount; ++f) {
            if (!assigned[f]) {
                restFaces.emplace_back(static_cast<int>(f));
            }
        }

        std::vector<UsdSubMeshGroup> groups;
        if (!restFaces.empty() || subsets.empty()) {
            groups.push_back(UsdSubMeshGroup{restMaterial, std::move(restFaces)});
        }
        for (auto &subset : subsets) {
            if (!subset.faces.empty()) {
                groups.emplace_back(std::move(subset));
            }
        }
        return groups;
    }

    static Uuid ProcessUsdMesh(const pxr::UsdGeomMesh &mesh, UsdBuildContext &context, const AssetImportRequest &request)
    {
        std::string meshName = mesh.GetPrim().GetName().GetString() + ".mesh";

        pxr::VtArray<pxr::GfVec3f> points;
        mesh.GetPointsAttr().Get(&points);
        pxr::VtArray<int> faceCounts;
        mesh.GetFaceVertexCountsAttr().Get(&faceCounts);
        pxr::VtArray<int> faceIndices;
        mesh.GetFaceVertexIndicesAttr().Get(&faceIndices);
        if (points.empty() || faceCounts.empty() || faceIndices.empty()) {
            LOG_W(TAG, "empty mesh %s", mesh.GetPrim().GetPath().GetString().c_str());
            return {};
        }

        pxr::UsdGeomPrimvarsAPI pvApi(mesh);

        // 3ds Max 导出器使用 map1 而非标准 st，两个名字都要查
        pxr::UsdGeomPrimvar uvPrimvar = pvApi.GetPrimvar(pxr::TfToken("st"));
        if (!uvPrimvar) {
            uvPrimvar = pvApi.GetPrimvar(pxr::TfToken("map1"));
        }
        pxr::VtArray<pxr::GfVec2f> uvs;
        pxr::VtArray<int> uvIndices;
        bool uvFaceVarying = false;
        if (uvPrimvar) {
            uvPrimvar.Get(&uvs);
            if (uvPrimvar.IsIndexed()) {
                uvPrimvar.GetIndices(&uvIndices);
            }
            uvFaceVarying = uvPrimvar.GetInterpolation() == pxr::UsdGeomTokens->faceVarying;
        }

        pxr::VtArray<pxr::GfVec3f> normals;
        pxr::VtArray<int> normalIndices;
        bool normalFaceVarying = false;
        if (auto nPrimvar = pvApi.GetPrimvar(pxr::TfToken("normals"))) {
            nPrimvar.Get(&normals);
            if (nPrimvar.IsIndexed()) {
                nPrimvar.GetIndices(&normalIndices);
            }
            normalFaceVarying = nPrimvar.GetInterpolation() == pxr::UsdGeomTokens->faceVarying;
        }

        // 每个面的起始角点
        std::vector<size_t> faceCornerBase(faceCounts.size() + 1, 0);
        for (size_t f = 0; f < faceCounts.size(); ++f) {
            faceCornerBase[f + 1] = faceCornerBase[f] + static_cast<size_t>(faceCounts[f]);
        }
        size_t cornerCount = faceCornerBase.back();

        // 按 face-vertex（角点）展开成 per-vertex，UV/法线索引可能与顶点索引不一致
        std::vector<Vector4> position;
        std::vector<StandardVertexData> ext;
        std::vector<uint32_t> indices;
        position.reserve(cornerCount);
        ext.reserve(cornerCount);
        indices.reserve(faceCounts.size() * 6); // 四边形扇形三角化的上界估计

        MeshAssetData meshData;

        auto groups = CollectSubMeshGroups(mesh);
        for (const auto &group : groups) {
            Uuid matUuid = group.material ? ProcessUsdMaterial(group.material, context, request) : GetDefaultMaterial(context);

            size_t matIndex = meshData.materials.size();
            auto iter = std::find(meshData.materials.begin(), meshData.materials.end(), matUuid);
            if (iter != meshData.materials.end()) {
                matIndex = std::distance(meshData.materials.begin(), iter);
            } else {
                meshData.materials.emplace_back(matUuid);
            }

            MeshSubSection subMesh = {};
            subMesh.firstVertex = static_cast<uint32_t>(position.size());
            subMesh.firstIndex = static_cast<uint32_t>(indices.size());
            subMesh.materialIndex = static_cast<uint32_t>(matIndex);
            bool firstPoint = true;

            for (int f : group.faces) {
                size_t cornerBase = faceCornerBase[f];
                int count = faceCounts[f];
                for (int c = 0; c < count; ++c) {
                    size_t corner = cornerBase + static_cast<size_t>(c);
                    int vi = faceIndices[corner];
                    const auto &p = points[vi];

                    position.emplace_back(p[0], p[1], p[2], 1.f);
                    if (firstPoint) {
                        subMesh.aabb.min = subMesh.aabb.max = Vector3(p[0], p[1], p[2]);
                        firstPoint = false;
                    } else {
                        subMesh.aabb.min = Min(subMesh.aabb.min, Vector3(p[0], p[1], p[2]));
                        subMesh.aabb.max = Max(subMesh.aabb.max, Vector3(p[0], p[1], p[2]));
                    }

                    StandardVertexData vtx = {};

                    if (!uvs.empty()) {
                        size_t uvi = uvFaceVarying ? corner : static_cast<size_t>(vi);
                        if (!uvIndices.empty()) {
                            uvi = static_cast<size_t>(uvIndices[corner]);
                        }
                        if (uvi < uvs.size()) {
                            vtx.uv = Vector4(uvs[uvi][0], uvs[uvi][1], 0.f, 0.f);
                        }
                    }

                    if (!normals.empty()) {
                        size_t ni = normalFaceVarying ? corner : static_cast<size_t>(vi);
                        if (!normalIndices.empty()) {
                            ni = static_cast<size_t>(normalIndices[corner]);
                        }
                        if (ni >= normals.size()) {
                            ni = 0;
                        }
                        vtx.normal = Vector4(normals[ni][0], normals[ni][1], normals[ni][2], 1.f);
                    } else {
                        vtx.normal = Vector4(0.f, 1.f, 0.f, 1.f);
                    }

                    vtx.tangent = Vector4(1.f, 0.f, 0.f, 1.f);
                    vtx.color = Vector4(1.f, 1.f, 1.f, 1.f);
                    ext.emplace_back(vtx);
                }

                // 扇形三角化（0,1,2 / 0,2,3 / ...）
                // 注意：运行时 DrawIndexed 会以 firstVertex 作为 vertexOffset，
                // 索引必须是段内局部索引（与 PrefabBuilder/assimp 约定一致）
                uint32_t base = static_cast<uint32_t>(position.size()) - static_cast<uint32_t>(count) - subMesh.firstVertex;
                for (int c = 1; c + 1 < count; ++c) {
                    indices.emplace_back(base);
                    indices.emplace_back(base + static_cast<uint32_t>(c));
                    indices.emplace_back(base + static_cast<uint32_t>(c) + 1);
                }
            }

            subMesh.vertexCount = static_cast<uint32_t>(position.size()) - subMesh.firstVertex;
            subMesh.indexCount = static_cast<uint32_t>(indices.size()) - subMesh.firstIndex;

            if (meshData.subMeshes.empty()) {
                meshData.aabb = subMesh.aabb;
            } else {
                meshData.aabb.min = Min(meshData.aabb.min, subMesh.aabb.min);
                meshData.aabb.max = Max(meshData.aabb.max, subMesh.aabb.max);
            }
            meshData.subMeshes.emplace_back(subMesh);
        }

        // 按三角形从 UV 梯度计算切线（索引为段内局部索引，需映射回全局顶点）
        for (const auto &subMesh : meshData.subMeshes) {
            uint32_t vb = subMesh.firstVertex;
            size_t indexEnd = static_cast<size_t>(subMesh.firstIndex) + subMesh.indexCount;
            for (size_t t = subMesh.firstIndex; t + 2 < indexEnd; t += 3) {
                uint32_t i0 = indices[t] + vb;
                uint32_t i1 = indices[t + 1] + vb;
                uint32_t i2 = indices[t + 2] + vb;

                const Vector4 &p0 = position[i0];
                const Vector4 &p1 = position[i1];
                const Vector4 &p2 = position[i2];
                const Vector4 &uv0 = ext[i0].uv;
                const Vector4 &uv1 = ext[i1].uv;
                const Vector4 &uv2 = ext[i2].uv;

                Vector3 edge1(p1.x - p0.x, p1.y - p0.y, p1.z - p0.z);
                Vector3 edge2(p2.x - p0.x, p2.y - p0.y, p2.z - p0.z);
                float du1 = uv1.x - uv0.x, dv1 = uv1.y - uv0.y;
                float du2 = uv2.x - uv0.x, dv2 = uv2.y - uv0.y;

                float det = du1 * dv2 - du2 * dv1;
                Vector3 tangent(1.f, 0.f, 0.f);
                Vector3 biTangent(0.f, 1.f, 0.f);
                if (std::fabs(det) > FLT_EPSILON) {
                    float f = 1.f / det;
                    tangent = (edge1 * dv2 - edge2 * dv1) * f;
                    biTangent = (edge2 * du1 - edge1 * du2) * f;
                }
                tangent.Normalize();

                for (uint32_t idx : {i0, i1, i2}) {
                    const Vector4 &n = ext[idx].normal;
                    Vector3 normal(n.x, n.y, n.z);
                    float w = normal.Cross(tangent).Dot(biTangent) < 0.f ? -1.f : 1.f;
                    ext[idx].tangent = Vector4(tangent.x, tangent.y, tangent.z, w);
                }
            }
        }

        size_t vtxCount = position.size();
        auto posSize = static_cast<uint32_t>(vtxCount * sizeof(Vector4));
        auto stdSize = static_cast<uint32_t>(vtxCount * sizeof(StandardVertexData));

        meshData.buffers = {
            MeshBufferView{0,       posSize, sizeof(Vector4)},
            MeshBufferView{posSize, stdSize, sizeof(StandardVertexData)},
        };

        meshData.attributes = {
            VertexAttribute{VertexSemanticFlagBit::POSITION, 0, 0, rhi::Format::F_RGBA32},
            VertexAttribute{VertexSemanticFlagBit::UV, 1, OFFSET_OF(StandardVertexData, uv), rhi::Format::F_RGBA32},
            VertexAttribute{VertexSemanticFlagBit::NORMAL, 1, OFFSET_OF(StandardVertexData, normal), rhi::Format::F_RGBA32},
            VertexAttribute{VertexSemanticFlagBit::TANGENT, 1, OFFSET_OF(StandardVertexData, tangent), rhi::Format::F_RGBA32},
            VertexAttribute{VertexSemanticFlagBit::COLOR, 1, OFFSET_OF(StandardVertexData, color), rhi::Format::F_RGBA32},
        };

        uint32_t idxOffset = posSize + stdSize;
        auto idxSize = static_cast<uint32_t>(indices.size() * sizeof(uint32_t));
        meshData.indexBuffer = static_cast<uint32_t>(meshData.buffers.size());
        meshData.indexType   = rhi::IndexType::U32;
        meshData.buffers.emplace_back(MeshBufferView{idxOffset, idxSize, static_cast<uint32_t>(sizeof(uint32_t))});

        meshData.dataSize = idxOffset + idxSize;
        meshData.rawData.storage.resize(meshData.dataSize);

        memcpy(meshData.rawData.storage.data(), reinterpret_cast<const char*>(position.data()), posSize);
        memcpy(meshData.rawData.storage.data() + posSize, reinterpret_cast<const char*>(ext.data()), stdSize);
        memcpy(meshData.rawData.storage.data() + idxOffset, reinterpret_cast<const char*>(indices.data()), idxSize);

        AssetSourcePath sourcePath = {};
        sourcePath.bundle = SourceAssetBundle::WORKSPACE;
        sourcePath.path = context.path.path / FilePath(meshName);

        {
            auto file = AssetDataBase::Get()->CreateOrOpenFile(sourcePath);
            auto archive = file->WriteAsArchive();
            BinaryOutputArchive bin(*archive);
            meshData.Save(bin);
        }

        auto source = AssetDataBase::Get()->RegisterAsset(sourcePath);
        context.meshes.emplace_back(source);

        // 与 PrefabBuilder 一致：为每个 mesh 生成单级 LOD 的 .lodgroup
        std::string lodName = meshName;
        auto dotPos = lodName.find_last_of('.');
        if (dotPos != std::string::npos) {
            lodName = lodName.substr(0, dotPos);
        }
        lodName += ".lodgroup";

        LodGroupData lodData = {};
        lodData.type = "mesh";
        LodGroupLevelData level = {};
        level.screenSize = 0.f;
        level.resId = source->uuid;
        lodData.levels.emplace_back(level);

        AssetSourcePath lodPath = {};
        lodPath.bundle = SourceAssetBundle::WORKSPACE;
        lodPath.path = context.path.path / FilePath(lodName);
        {
            auto file = AssetDataBase::Get()->CreateOrOpenFile(lodPath);
            auto archive = file->WriteAsArchive();
            JsonOutputArchive json(*archive);
            lodData.SaveJson(json);
        }
        auto lodSource = AssetDataBase::Get()->RegisterAsset(lodPath);
        return lodSource->uuid;
    }

    void UsdBuilder::Import(const AssetImportRequest &request) const
    {
        SetupUsdPluginPath();

        auto stage = pxr::UsdStage::Open(request.filePath.GetStr());
        if (!stage) {
            LOG_E(TAG, "failed to open usd stage %s", request.filePath.GetStr().c_str());
            return;
        }

        double metersPerUnit = pxr::UsdGeomGetStageMetersPerUnit(stage);
        float unitScale = static_cast<float>(metersPerUnit);

        auto upAxis = pxr::UsdGeomGetStageUpAxis(stage);
        if (upAxis == pxr::UsdGeomTokens->z) {
            LOG_W(TAG, "z-up stage is not handled yet, result may need a -90 deg rotation on X");
        }

        UsdBuildContext context;
        context.path.bundle = SourceAssetBundle::WORKSPACE;
        context.path.path = FilePath("Prefabs") / request.filePath.FileName();
        context.name = request.filePath.FileNameWithoutExt();

        AssetDataBase::Get()->GetWorkSpaceFs()->CreateSubSystem(context.path.path.GetStr(), true);

        // 根节点：metersPerUnit 烘焙进根变换（如 0.01，厘米 -> 米）
        {
            RenderPrefabNode root = {};
            root.name = context.name;
            root.parentIndex = ~(0u);
            root.localTransform.scale = Vector3(unitScale, unitScale, unitScale);
            context.nodes.emplace_back(root);
        }

        for (const auto &prim : stage->Traverse()) {
            if (!prim.IsA<pxr::UsdGeomMesh>()) {
                continue;
            }
            pxr::UsdGeomMesh mesh(prim);

            auto &node = context.nodes.emplace_back();
            node.name = prim.GetName().GetString();
            node.parentIndex = 0;

            pxr::UsdGeomXformable xformable(prim);
            Matrix4 local = FromUsd(xformable.ComputeLocalToWorldTransform(pxr::UsdTimeCode::Default()));
            Decompose(local, node.localTransform.translation, node.localTransform.rotation, node.localTransform.scale);

            node.mesh = ProcessUsdMesh(mesh, context, request);
        }

        RenderPrefabAssetData data;
        data.nodes.swap(context.nodes);

        AssetSourcePath sourcePath = {};
        sourcePath.bundle = SourceAssetBundle::WORKSPACE;
        sourcePath.path = context.path.path / FilePath(context.name + ".prefab");

        {
            auto file = AssetDataBase::Get()->CreateOrOpenFile(sourcePath);
            auto archive = file->WriteAsArchive();
            JsonOutputArchive bin(*archive);
            data.SaveJson(bin);
        }

        AssetDataBase::Get()->RegisterAsset(sourcePath);
        LOG_I(TAG, "usd import finished %s", sourcePath.path.GetStr().c_str());
    }

} // namespace sky::builder

#else

namespace sky::builder {
    void UsdBuilder::Import(const AssetImportRequest &request) const
    {
        LOG_E(TAG, "UsdBuilder is disabled (SKY_WITH_USD not defined), %s skipped", request.filePath.GetStr().c_str());
    }
} // namespace sky::builder

#endif
