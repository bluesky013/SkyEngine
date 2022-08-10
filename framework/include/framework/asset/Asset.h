//
// Created by yjrj on 2022/8/8.
//

#pragma once

#include <core/util/Uuid.h>
#include <core/jobsystem/JobSystem.h>
#include <memory>
#include <vector>
#include <string>

namespace sky {

    class AssetBase {
    public:
        AssetBase() = default;
        virtual ~AssetBase() = default;

        enum class Status {
            INITIAL,
            LOADING,
            LOADED
        };

        void SetUuid(const Uuid& id);

        const Uuid& GetUuid() const;

        Status GetStatus() const;

        void BlockUtilLoaded()
        {
            if (future.valid()) {
                future.wait();
            }
        }

    private:
        friend class AssetManager;
        Uuid uuid;
        Status status = Status::INITIAL;
        tf::Future<void> future;
    };
    using AssetPtr = std::shared_ptr<AssetBase>;

    template <typename T>
    struct AssetTraits {
        using DataType = std::vector<uint8_t>;

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

        T* CreateInstance()
        {
            return AssetTraits<T>::CreateFromData(data);
        }

    private:
        friend class AssetManager;
        DataType data;
    };
}