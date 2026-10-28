#include "PhysicsAssetCodec.h"
#include <builder/physics/PhysicsAssetBuilder.h>

#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>

namespace sky::phy {
    const std::vector<std::string> &PhysicsAssetBuilder::GetExtensions() const
    {
        static const std::vector<std::string> extensions{".collision", ".physmat"};
        return extensions;
    }

    std::string_view PhysicsAssetBuilder::QueryType(const std::string &extension) const
    {
        return extension == ".collision" ? COLLISION_ASSET_TYPE : extension == ".physmat" ? MATERIAL_ASSET_TYPE : std::string_view{};
    }

    void PhysicsAssetBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        result.retCode = AssetBuildRetCode::FAILED;
        result.error   = "Invalid physics asset build request";
        if (!request.assetInfo || !request.assetInfo->uuid || !request.file)
            return;
        result.uuid        = request.assetInfo->uuid;
        result.target      = request.target;
        const auto  type   = QueryType(request.assetInfo->ext);
        auto        stream = request.file->ReadAsArchive();
        std::string source, encoded;
        if (type.empty() || !stream || !ReadPhysicsText(stream->GetStream(), source))
            return;
        PhysicsStatus status;
        if (type == COLLISION_ASSET_TYPE) {
            CollisionSource geometry;
            status = ParseCollision(source, geometry);
            if (status)
                status = WriteCollision(geometry, encoded);
        } else {
            PhysicsMaterialData material;
            status = ParsePhysicsMaterial(source, material);
            if (status)
                status = WritePhysicsMaterial(material, encoded);
        }
        if (!status) {
            result.error = status.message;
            return;
        }
        const AssetProductData product{request.assetInfo->uuid, std::string(type), {}, {encoded.begin(), encoded.end()}};
        const auto            &bundle    = request.bundle.empty() ? request.target : request.bundle;
        const auto             published = AssetManager::Get()->SaveProductData(std::span(&product, 1), bundle);
        if (!published) {
            result.error = published.message;
            return;
        }
        request.assetInfo->dependencies.clear();
        AssetDataBase::Get()->SetBuildDependencies(product.uuid, {});
        result.error.clear();
        result.retCode = AssetBuildRetCode::SUCCESS;
    }
} // namespace sky::phy
