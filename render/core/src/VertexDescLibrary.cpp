//
// Created by Zach Lee on 2023/9/9.
//

#include <render/VertexDescLibrary.h>

namespace sky {

    void VertexDescLibrary::RegisterVertexDesc(const std::string &key, const rhi::VertexInputPtr &desc)
    {
        descriptions.emplace(key, desc);
    }

    rhi::VertexInputPtr VertexDescLibrary::FindVertexDesc(const std::string &key) const
    {
        auto iter = descriptions.find(key);
        return iter != descriptions.end() ? iter->second : nullptr;
    }

} // namespace sky
