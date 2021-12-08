//
// Created by Zach Lee on 2021/12/5.
//

#pragma once

#include <framework/asset/Asset.h>

namespace sky {

    class Bundle : public AssetInstanceBase {
    public:
        Bundle() = default;
        ~Bundle() = default;

        static constexpr Uuid TYPE = Uuid::CreateFromString("d9f4b48f-5d35-4d02-a5fe-ac975e663eff");

    private:
        const Uuid& GetType() const override { return TYPE; }
    };

}