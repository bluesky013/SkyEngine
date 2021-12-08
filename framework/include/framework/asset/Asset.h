//
// Created by Zach Lee on 2021/12/3.
//

#pragma once

#include <core/util/Rtti.h>
#include <core/util/Uuid.h>

namespace sky {

    struct AssetHead {
        uint32_t magic = 0x00594B53;
        Uuid type = {};
        Uuid id = {};
    };

    class AssetInstanceBase {
    public:
        AssetInstanceBase() = default;
        virtual ~AssetInstanceBase() = default;

        enum class Status : uint8_t {
            UNLOAD,
            LOADING,
            LOADED
        };

        bool IsReady() const { return status == Status::LOADED; }

        const Uuid& GetId() const { return id; }

        virtual const Uuid& GetType() const = 0;

    protected:
        friend class AssetManager;
        Uuid id;
        Status status = Status::UNLOAD;
    };

    class AssetHandlerBase {
    public:
        AssetHandlerBase() = default;
        virtual ~AssetHandlerBase() = default;

        virtual AssetInstanceBase* Create(const Uuid& id) = 0;
    };

    template <typename T>
    class AssetHandler : public AssetHandlerBase {
    public:
        AssetHandler() = default;
        ~AssetHandler() = default;

        AssetInstanceBase* Create(const Uuid& id) override
        {
            return new T();
        }
    };

    template <typename T>
    class Asset {
    public:
        Asset(const Uuid& id)
            : assetId(id)
            , typeId(T::TYPE)
            , instance(nullptr)
            {}

        Asset() = default;
        ~Asset() = default;

        T* Get() const
        {
            return instance;
        }

        const Uuid& GetId() const
        {
            return assetId;
        }

    private:
        friend class AssetManager;

        Uuid assetId;
        Uuid typeId;
        T* instance;
    };

}
