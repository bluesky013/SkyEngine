//
// Created by Zach Lee on 2023/9/9.
//

#pragma once

#include <string>
#include <unordered_map>
#include <rhi/VertexInput.h>

namespace sky {

    class VertexDescLibrary {
    public:
        VertexDescLibrary() = default;
        ~VertexDescLibrary() = default;

        void RegisterVertexDesc(const std::string &key, const rhi::VertexInputPtr &desc);
        rhi::VertexInputPtr FindVertexDesc(const std::string &key) const;

    private:
        std::unordered_map<std::string, rhi::VertexInputPtr> descriptions;
    };

} // namespace sky
