//
// Created by yjrj on 2022/8/8.
//

#pragma once

#include <core/util/Uuid.h>
#include <memory>
#include <vector>

namespace sky {

    class AssetBase {
    public:
        AssetBase() = default;
        virtual ~AssetBase() = default;

        void SetUuid(const Uuid& id);

        const Uuid& GetUuid() const;

    private:
        Uuid uuid;
    };
    using AssetPtr = std::shared_ptr<AssetBase>;

    template <typename T>
    struct AssetTraits {
        using DataType = std::vector<uint8_t>;

        static constexpr char* EXT = "asset";

        static void LoadFromPath(const std::string& path, DataType& data)
        {
        }

        static T* CreateFromData(const DataType& data)
        {
            return nullptr;
        }
    };

    template <typename T>
    class Asset : public AssetBase {
    public:
        Asset() = default;
        ~Asset() = default;

        using DataType = typename AssetTraits<T>::DataType;



    private:
        DataType data;
    };
}