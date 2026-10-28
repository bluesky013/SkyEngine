#include "ProductAudit.h"

#include <algorithm>
#include <filesystem>
#include <framework/asset/AssetManager.h>
#include <map>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <string_view>

using namespace sky;

namespace {
    bool SafeBundle(std::string_view value)
    {
        return !value.empty() && value != "." && value != ".." && value.find_first_of("/\\:") == std::string_view::npos;
    }
    bool SafeProductPath(std::string_view value)
    {
        if (value.empty() || value.size() > 4096 || value.front() == '/' || value.find_first_of("\\:") != std::string_view::npos ||
            value.find('\0') != std::string_view::npos)
            return false;
        while (!value.empty()) {
            const auto separator = value.find('/');
            const auto segment   = value.substr(0, separator);
            if (segment.empty() || segment == "." || segment == "..")
                return false;
            if (separator == std::string_view::npos)
                return true;
            value.remove_prefix(separator + 1);
        }
        return false;
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
    std::vector<std::string> bundles;
    for (const auto &entry : manifest["bundles"].GetArray()) {
        if (!entry.IsString() || !SafeBundle(entry.GetString()))
            return Invalid("Invalid package bundle name");
        const std::string name = entry.GetString();
        if (std::find(bundles.begin(), bundles.end(), name) != bundles.end())
            return Invalid("Duplicate package bundle name");
        FileSystemPtr source = new NativeFileSystem((path.parent_path() / name).string());
        AssetManager::Get()->AddAssetProductBundle(new PackageAssetBundle(source, name));
        bundles.push_back(name);
    }
    struct Request {
        Uuid        uuid;
        std::string type;
    };
    std::vector<Request> pending;
    for (const auto &entry : manifest["roots"].GetArray()) {
        if (!entry.IsObject() || !entry.HasMember("type") || !entry["type"].IsString() || entry.HasMember("uuid") == entry.HasMember("path"))
            return Invalid("Malformed World asset root");
        Uuid uuid;
        if (entry.HasMember("uuid")) {
            if (!entry["uuid"].IsString())
                return Invalid("World asset root UUID must be a string");
            uuid = Uuid::CreateFromString(entry["uuid"].GetString());
        } else {
            if (!entry["path"].IsString() || !SafeProductPath({entry["path"].GetString(), entry["path"].GetStringLength()}))
                return Invalid("World product root must use a safe logical path");
            uuid = AssetManager::Get()->ResolveProductPath({entry["path"].GetString(), entry["path"].GetStringLength()});
            if (!uuid)
                return Invalid("World product path is missing or ambiguous: " + std::string(entry["path"].GetString()));
        }
        const std::string_view type{entry["type"].GetString(), entry["type"].GetStringLength()};
        if (!uuid || type.empty() || type.size() > 4096 || type.find('\0') != std::string_view::npos)
            return Invalid("World asset root has no identity or type");
        pending.push_back({uuid, std::string(type)});
    }
    struct Product {
        std::string            bundle;
        AssetProductInspection asset;
    };
    std::map<Uuid, Product> products;
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
            status = AssetManager::Get()->InspectProduct(request.uuid, bundle, product.asset);
            if (status || status.error != ProductError::NotFound) {
                product.bundle = bundle;
                break;
            }
        }
        if (!status)
            return {status.error, request.uuid.ToString() + ": " + status.message};
        if (!request.type.empty() && request.type != product.asset.type)
            return Invalid("World asset type mismatch for " + request.uuid.ToString());
        for (const auto &dependency : product.asset.dependencies)
            pending.push_back({dependency, {}});
        product.asset.payload.Reset(nullptr);
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
        writer.Key("formatVersion");
        writer.Uint(product.asset.formatVersion);
        writer.Key("checksum");
        writer.Uint64(product.asset.checksum);
        writer.Key("bulkSize");
        writer.Uint64(product.asset.bulkSize);
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
