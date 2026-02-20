//
// Created by SkyEngine on 2024/02/20.
//

#include <render/culling/PVSStreamingTypes.h>
#include <framework/serialization/BinaryArchive.h>

namespace sky {

    void PVSSectorBakedData::Save(BinaryOutputArchive &archive) const
    {
        // Save version
        archive.SaveValue(VERSION);
        
        // Save sector coordinate
        archive.SaveValue(coord.x);
        archive.SaveValue(coord.y);
        archive.SaveValue(coord.z);
        
        // Save bounds
        archive.SaveValue(bounds.min.x);
        archive.SaveValue(bounds.min.y);
        archive.SaveValue(bounds.min.z);
        archive.SaveValue(bounds.max.x);
        archive.SaveValue(bounds.max.y);
        archive.SaveValue(bounds.max.z);
        
        // Save PVS data
        pvsData.Save(archive);
        
        // Save object names
        uint32_t numNames = static_cast<uint32_t>(objectNames.size());
        archive.SaveValue(numNames);
        for (const auto &name : objectNames) {
            archive.SaveValue(name);
        }
    }

    void PVSSectorBakedData::Load(BinaryInputArchive &archive)
    {
        // Load and verify version
        uint32_t version = 0;
        archive.LoadValue(version);
        if (version != VERSION) {
            return;
        }
        
        // Load sector coordinate
        archive.LoadValue(coord.x);
        archive.LoadValue(coord.y);
        archive.LoadValue(coord.z);
        
        // Load bounds
        archive.LoadValue(bounds.min.x);
        archive.LoadValue(bounds.min.y);
        archive.LoadValue(bounds.min.z);
        archive.LoadValue(bounds.max.x);
        archive.LoadValue(bounds.max.y);
        archive.LoadValue(bounds.max.z);
        
        // Load PVS data
        pvsData.Load(archive);
        
        // Load object names
        uint32_t numNames = 0;
        archive.LoadValue(numNames);
        objectNames.resize(numNames);
        for (auto &name : objectNames) {
            archive.LoadValue(name);
        }
    }

    size_t PVSSectorBakedData::GetMemorySize() const
    {
        size_t size = sizeof(PVSSectorBakedData);
        size += pvsData.GetMemorySize();
        for (const auto &name : objectNames) {
            size += name.size();
        }
        return size;
    }

} // namespace sky
