//
// Created by Zach Lee on 2023/9/9.
//

#include <render/adaptor/assets/VertexDescLibraryAsset.h>
#include <framework/serialization/BinaryArchive.h>
#include <render/RHI.h>

namespace sky {

    void VertexDescLibraryAssetData::Load(BinaryInputArchive &archive)
    {
        uint32_t descCount = 0;
        archive.LoadValue(descCount);
        for (uint32_t i = 0; i < descCount; ++i) {
            std::string key;
            archive.LoadValue(key);
            uint32_t count = 0;

            auto &desc = descriptions[key];
            archive.LoadValue(count);
            desc.attributes.resize(count);
            for (uint32_t j = 0; j < count; ++j) {
                auto &attribute = desc.attributes[j];
                archive.LoadValue(attribute.binding);
                archive.LoadValue(attribute.offset);
                archive.LoadValue(attribute.location);
                archive.LoadValue(attribute.format);
            }
            archive.LoadValue(count);
            desc.bindings.resize(count);
            for (uint32_t j = 0; j < count; ++j) {
                auto &binding = desc.bindings[j];
                archive.LoadValue(binding.binding);
                archive.LoadValue(binding.stride);
                archive.LoadValue(binding.inputRate);
            }
        }
    }

    void VertexDescLibraryAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(static_cast<uint32_t>(descriptions.size()));
        for (const auto &[key, desc] : descriptions) {
            archive.SaveValue(key);
            archive.SaveValue(static_cast<uint32_t>(desc.attributes.size()));
            for (const auto &attribute : desc.attributes) {
                archive.SaveValue(attribute.binding);
                archive.SaveValue(attribute.offset);
                archive.SaveValue(attribute.location);
                archive.SaveValue(attribute.format);
            }
            archive.SaveValue(static_cast<uint32_t>(desc.bindings.size()));
            for (const auto &binding : desc.bindings) {
                archive.SaveValue(binding.binding);
                archive.SaveValue(binding.stride);
                archive.SaveValue(binding.inputRate);
            }
        }
    }

    VertexDescLibrary *CreateVertexDescLibrary(const VertexDescLibraryAssetData &data)
    {
        auto *vtxLib = new VertexDescLibrary();
        auto *device = RHI::Get()->GetDevice();
        for (const auto &[key, desc] : data.descriptions) {
            vtxLib->RegisterVertexDesc(key, device->CreateVertexInput(desc));
        }
        return vtxLib;
    }
} // namespace sky
