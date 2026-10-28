#include <animation/assets/GraphAsset.h>
#include <animation/builder/GraphBuilder.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>

namespace sky::animation {
    std::string_view GraphBuilder::QueryType(const std::string &extension) const
    {
        return extension == ".graph" ? AssetTraits<GraphAsset>::ASSET_TYPE : std::string_view{};
    }
    void GraphBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        result.retCode = AssetBuildRetCode::FAILED;
        if (!request.file || !request.assetInfo) {
            result.error = "Graph source file or identity is missing";
            return;
        }
        auto       *manager = AssetManager::Get();
        const auto &bundle  = request.bundle.empty() ? request.target : request.bundle;
        const auto input = request.file->ReadAsArchive();
        if (!input) {
            result.error = "Graph source cannot be opened";
            return;
        }
        auto &stream = input->GetStream();
        stream.seekg(0, std::ios::end);
        const auto length = stream.tellg();
        if (length <= 0 || length > 8u * 1024u * 1024u) {
            result.error = "Graph source size is invalid";
            return;
        }
        stream.seekg(0, std::ios::beg);
        std::string text(static_cast<size_t>(length), '\0');
        if (!input->LoadRaw(text.data(), text.size())) {
            result.error = "Graph source read failed";
            return;
        }
        GraphSource source;
        auto        valid = ParseGraphSource(text, source, request.assetInfo->uuid);
        if (!valid) {
            result.error = valid.diagnostics.front().message;
            return;
        }
        CookedGraph compiled;
        valid = CompileGraphDefinition(source.definition, compiled);
        if (!valid) {
            result.error = valid.diagnostics.front().object.ToString() + ": " + valid.diagnostics.front().message;
            return;
        }
        auto      *catalog = AssetDataBase::Get();
        const auto hasType = [&](const Uuid &id, std::string_view expected) {
            std::string type;
            return catalog->Exists(id) && catalog->GetType(id, type) && type == expected;
        };
        if (!hasType(compiled.definition.skeleton, "AuroraSkeleton")) {
            result.error = "Graph Skeleton is missing or has the wrong type";
            return;
        }
        for (const auto &node : compiled.definition.nodes) {
            if (node.kind == GraphNodeKind::Clip && !hasType(node.clip, "AnimationClip")) {
                result.error = "Graph Clip is missing or has the wrong type: " + node.clip.ToString();
                return;
            }
        }
        const auto asset = std::static_pointer_cast<Asset<GraphAsset>>(manager->CreateBuildAsset(request.assetInfo->uuid, Name("AnimationGraph")));
        if (!asset) {
            result.error = "Animation Graph handler is unavailable";
            return;
        }
        asset->Data().graph = std::move(compiled);
        for (const auto &dependency : asset->Data().graph.dependencies) {
            asset->AddDependencies(dependency);
        }
        const auto saved = manager->SaveAsset(asset, bundle);
        if (!saved) {
            result.error = saved.message;
            return;
        }
        catalog->SetBuildDependencies(request.assetInfo->uuid, asset->Data().graph.dependencies);
        request.assetInfo->dependencies = asset->Data().graph.dependencies;
        result.retCode                  = AssetBuildRetCode::SUCCESS;
        result.error.clear();
    }
} // namespace sky::animation
