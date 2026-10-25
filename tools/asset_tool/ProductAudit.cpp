#include "ProductAudit.h"

#include <algorithm>
#include <filesystem>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/JsonArchive.h>
#include <map>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

using namespace sky;

namespace {
    bool SafeBundle(std::string_view value)
    {
        return !value.empty() && value != "." && value != ".." && value.find_first_of("/\\:") == std::string_view::npos;
    }
    ProductStatus Invalid(std::string message)
    {
        return {ProductError::InvalidData, std::move(message)};
    }
} // namespace

ProductStatus AuditPackagedProducts(const std::string &manifestPath)
{
    const auto                path  = std::filesystem::path(manifestPath);
    const NativeFileSystemPtr fs    = new NativeFileSystem(path.parent_path().string());
    const auto                input = fs->OpenFile(path.filename().string());
    std::string               text;
    if (!input || !input->ReadString(text))
        return {ProductError::IOError, "Cannot read package asset roots"};
    rapidjson::Document manifest;
    manifest.Parse(text.c_str());
    if (manifest.HasParseError() || !manifest.IsObject() || !manifest.HasMember("bundles") || !manifest["bundles"].IsArray() ||
        !manifest.HasMember("roots") || !manifest["roots"].IsArray() || manifest["bundles"].Size() > 256 || manifest["roots"].Size() > 100000)
        return Invalid("Package roots must contain bundles and roots arrays");
    struct Bundle {
        std::string   name;
        FileSystemPtr fs;
    };
    std::vector<Bundle> bundles;
    for (const auto &entry : manifest["bundles"].GetArray()) {
        if (!entry.IsString() || !SafeBundle(entry.GetString()))
            return Invalid("Invalid package bundle name");
        const std::string name = entry.GetString();
        if (std::any_of(bundles.begin(), bundles.end(), [&](const auto &item) { return item.name == name; }))
            return Invalid("Duplicate package bundle name");
        FileSystemPtr source = new NativeFileSystem((path.parent_path() / name).string());
        AssetManager::Get()->AddAssetProductBundle(new PackageAssetBundle(source, name));
        bundles.push_back({name, std::move(source)});
    }
    struct Request {
        Uuid        uuid;
        std::string type;
    };
    std::vector<Request> pending;
    for (const auto &entry : manifest["roots"].GetArray()) {
        if (!entry.IsObject() || !entry.HasMember("uuid") || !entry["uuid"].IsString() || !entry.HasMember("type") || !entry["type"].IsString())
            return Invalid("Malformed World asset root");
        const auto uuid = Uuid::CreateFromString(entry["uuid"].GetString());
        if (!uuid || entry["type"].GetStringLength() == 0 || entry["type"].GetStringLength() > 4096)
            return Invalid("World asset root has no identity or type");
        pending.push_back({uuid, entry["type"].GetString()});
    }
    struct Product {
        std::string            bundle;
        AssetProductInspection asset;
    };
    std::map<Uuid, Product>                                         products;
    std::map<std::pair<std::string, Uuid>, ProductGroupSnapshotPtr> groups;
    for (size_t index = 0; index < pending.size(); ++index) {
        if (pending.size() > 100000)
            return Invalid("Package dependency limit exceeded");
        const auto request = pending[index];
        if (const auto found = products.find(request.uuid); found != products.end()) {
            if (!request.type.empty() && request.type != found->second.asset.type)
                return Invalid("World asset type mismatch for " + request.uuid.ToString());
            continue;
        }
        Product       product;
        ProductStatus status{ProductError::NotFound, "Product missing from selected bundles"};
        for (const auto &bundle : bundles) {
            const bool owned = AssetProductGroupStore(bundle.fs).HasOwnerRecord(request.uuid);
            status           = AssetManager::Get()->InspectProduct(request.uuid, bundle.name, product.asset);
            if (status || owned || status.error != ProductError::NotFound) {
                product.bundle = bundle.name;
                break;
            }
        }
        if (!status)
            return {status.error, request.uuid.ToString() + ": " + status.message};
        if (!request.type.empty() && request.type != product.asset.type)
            return Invalid("World asset type mismatch for " + request.uuid.ToString());
        for (const auto &dependency : product.asset.dependencies)
            pending.push_back({dependency, {}});
        if (const auto &group = product.asset.group) {
            const auto key               = std::make_pair(product.bundle, group->owner);
            const auto [found, inserted] = groups.emplace(key, group);
            if (!inserted && found->second->generation != group->generation)
                return {ProductError::StaleRequest, "Product group changed during package audit"};
            if (inserted)
                for (const auto &member : group->members)
                    pending.push_back({member.uuid, {}});
        }
        products.emplace(request.uuid, std::move(product));
    }
    rapidjson::StringBuffer                    output;
    rapidjson::Writer<rapidjson::StringBuffer> writer(output);
    writer.StartObject();
    writer.Key("products");
    writer.StartArray();
    for (const auto &[uuid, product] : products) {
        writer.StartObject();
        writer.Key("uuid");
        writer.String(uuid.ToString().c_str());
        writer.Key("type");
        writer.String(product.asset.type.c_str());
        writer.Key("bundle");
        writer.String(product.bundle.c_str());
        writer.EndObject();
    }
    writer.EndArray();
    writer.Key("groups");
    writer.StartArray();
    for (const auto &[key, group] : groups) {
        writer.StartObject();
        writer.Key("bundle");
        writer.String(key.first.c_str());
        writer.Key("owner");
        writer.String(group->owner.ToString().c_str());
        writer.Key("generation");
        writer.String(group->generation.ToString().c_str());
        writer.Key("revision");
        writer.Uint64(group->revision);
        writer.EndObject();
    }
    writer.EndArray();
    writer.EndObject();
    const auto report = fs->CreateOrOpenFile("asset-closure.json");
    const auto stream = report ? report->WriteAsArchive() : OStreamArchivePtr{};
    if (!stream || !stream->SaveRaw(output.GetString(), output.GetSize()))
        return {ProductError::IOError, "Cannot write verified package closure"};
    stream->GetStream().flush();
    if (!stream->GetStream())
        return {ProductError::IOError, "Cannot flush verified package closure"};
    return {};
}
