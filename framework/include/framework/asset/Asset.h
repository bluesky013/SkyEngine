//
// Created by Zach Lee on 2021/12/3.
//

#pragma once

#include <core/util/Rtti.h>
#include <core/util/Uuid.h>
#include <core/template/ReferenceObject.h>

namespace sky {

    struct AssetHead {
        uint32_t magic = 0x00594B53;
        Uuid type = {};
        Uuid id = {};
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

    protected:
        friend class AssetManager;
        Uuid uuid;
        Status status = Status::UNLOAD;
    };

    using AssetPtr = CounterPtr<AssetBase>;

    class AssetHandlerBase {
    public:
        AssetHandlerBase() = default;
        virtual ~AssetHandlerBase() = default;

        virtual AssetPtr Create(const Uuid& id) = 0;

        virtual AssetPtr Load(const std::string&) = 0;
    };
}
