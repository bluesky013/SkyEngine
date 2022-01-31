//
// Created by Zach Lee on 2021/12/3.
//

#pragma once

#include <core/util/Rtti.h>
#include <core/util/Uuid.h>
#include <core/template/ReferenceObject.h>
#include <framework/asset/Resource.h>

namespace sky {

    struct AssetHead {
        uint32_t magic = 0x00594B53;
        Uuid type = {};
        Uuid id = {};

        template <class Archive>
        void serialize(Archive& ar)
        {
            ar(magic, type, id);
        }
    };

    class AssetBase : public RefObject<AssetBase> {
    public:
        AssetBase(const Uuid& id) : uuid(id) {}
        virtual ~AssetBase() = default;

        enum class Status : uint8_t {
            UNLOAD,
            LOADING,
            LOADED
        };

        bool IsReady() const { return status == Status::LOADED; }

        const Uuid& GetId() const { return uuid; }

        virtual const Uuid& GetType() const = 0;

        virtual ResourceInstance CreateInstance(const Uuid&)
        {
            return ResourceInstance {};
        }

    protected:
        friend class AssetManager;

        void OnExpire() override;

        Uuid uuid;
        Status status = Status::UNLOAD;
        std::string path;
    };

    using AssetPtr = CounterPtr<AssetBase>;

    class AssetHandlerBase {
    public:
        AssetHandlerBase() = default;
        virtual ~AssetHandlerBase() = default;

        virtual AssetBase* Create(const Uuid& id) = 0;

        AssetBase* Load(const std::string& path);
    };

    template <class Asset>
    class AssetHandler : public AssetHandlerBase {
    public:
        AssetBase* Create(const Uuid& id)
        {
            return new Asset(id);
        }
    };
}
