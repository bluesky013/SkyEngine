//
// Created by Zach Lee on 2021/12/5.
//

#pragma once

#include <framework/asset/Asset.h>

namespace sky {

    class Buffer : public AssetDataBase {
    public:
        Buffer() = default;
        ~Buffer() = default;

        static constexpr Uuid TYPE = Uuid::CreateFromString("1ba844f8-3032-45de-8f6a-0010dd9f2656");

    private:
        const Uuid& GetType() const override { return TYPE; }
    };

}