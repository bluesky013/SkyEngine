//
// Created by Zach Lee on 2023/9/9.
//

#pragma once

#include <framework/asset/AssetBuilder.h>
#include <string_view>

namespace sky::builder {
    class VertexLibraryBuilder : public AssetBuilder {
    public:
        VertexLibraryBuilder() = default;
        ~VertexLibraryBuilder() = default;

        static constexpr std::string_view KEY = "GFX_VLIB";

        void Request(const BuildRequest &request, BuildResult &result) override;
    };
} // sky::builder
