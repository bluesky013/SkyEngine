//
// Created by yjrj on 2022/8/8.
//

#pragma once

#include <core/util/Uuid.h>
#include <core/jobsystem/JobSystem.h>
#include <memory>
#include <vector>
#include <string>
#include <mutex>

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

        virtual void SaveToPath(const std::string& path) const {}

    protected:
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

        static void SaveToPath(const std::string& path, const DataType& data)
        {
        }

        static std::shared_ptr<T> CreateFromData(const DataType& data)
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

        void SetData(const DataType& input)
        {
            data = input;
        }

        void SetData(DataType&& input)
        {
            data = std::move(input);
        }

        std::shared_ptr<T> CreateInstance(bool useDefault = true)
        {
            std::shared_ptr<T> res;
            if (useDefault) {
                std::lock_guard<std::mutex> lock(mutex);
                res = defaultInstance.lock();
                if (res) {
                    return res;
                }
                res = AssetTraits<T>::CreateFromData(data);
                defaultInstance = res;
                return res;
            }
            return AssetTraits<T>::CreateFromData(data);
        }

        DataType& Data()
        {
            return data;
        }

        void SaveToPath(const std::string& path) const override
        {
            AssetTraits<T>::SaveToPath(path, data);
        }

        template<class Archiver>
        void serialize(Archiver& ar)
        {
            ar(uuid);
        }
    private:
        friend class AssetManager;
        DataType data;

        std::mutex mutex;
        std::weak_ptr<T> defaultInstance;

    };
}