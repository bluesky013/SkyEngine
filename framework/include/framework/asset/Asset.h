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
    };

    class AssetHandlerBase {
    public:
        AssetHandlerBase() = default;
        virtual ~AssetHandlerBase() = default;

        virtual AssetInstanceBase* Create() = 0;
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
        Asset() = default;
        ~Asset() = default;

        static constexpr char* TYPE = TypeInfo<T>::Name();
        static constexpr uint32_t TYPE_ID = TypeInfo<T>::Hash();

    private:
        Uuid assetId;

        AssetInstanceBase* instance = nullptr;
    };

}
