//
// Created by Zach Lee on 2021/12/3.
//

#pragma once

#include <core/util/Rtti.h>
#include <core/util/Uuid.h>

namespace sky {

    class AssetInstanceBase {
    public:
        AssetInstanceBase() = default;
        virtual ~AssetInstanceBase() = default;

        enum class Status : uint8_t {
            UNLOAD,
            LOADING,
            LOADED
        };

        virtual uint32_t GetType() const = 0;

        bool IsReady() const { return status == Status::LOADED; }

        Uuid GetId() const { return id; }

    protected:
        friend class AssetManager;
        Uuid id;
        Status status = Status::UNLOAD;
    };

    class AssetHandlerBase {
    public:
        AssetHandlerBase() = default;
        virtual ~AssetHandlerBase() = default;

        virtual AssetInstanceBase* Create() = 0;

        virtual bool LoadAsset(const std::string& path) { return false; }
    };

    template <typename T>
    class AssetHandler : public AssetHandlerBase {
    public:
        AssetHandler() = default;
        ~AssetHandler() = default;

        AssetInstanceBase* Create() override
        {
            return new T();
        }
    };

    template <typename T>
    class Asset {
    public:
        Asset(Uuid id) : assetId(id) {}
        Asset() = default;
        ~Asset() = default;

        static constexpr char* TYPE = TypeInfo<T>::Name();
        static constexpr uint32_t TYPE_ID = TypeInfo<T>::Hash();

    private:
        friend class AssetManager;

        Uuid assetId;
        T* instance = nullptr;
    };

}
