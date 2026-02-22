//
// Created by blues on 2025/2/28.
//

#pragma once

#include <render/renderpass/RasterPass.h>
#include <render/adaptor/pipeline/DefaultPassConstants.h>

namespace sky {

    class EmptyPass : public RasterPass {
    public:
        EmptyPass();
        ~EmptyPass() override = default;
    };

};
