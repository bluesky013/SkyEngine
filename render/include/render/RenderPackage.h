//
// Created by Zach Lee on 2023/8/19.
//

#pragma once

namespace sky {

    struct RenderPackage {
    };

    class RenderPackageBuilder {
    public:
        explicit RenderPackageBuilder(RenderPackage &pkg) : package(pkg) {}
        ~RenderPackageBuilder() = default;

    private:
        RenderPackage &package;
    };

} // namespace sky
