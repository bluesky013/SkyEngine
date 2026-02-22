
//
// Created by blues on 2025/1/10.
//

#pragma once

#include <core/util/Uuid.h>
#include <core/environment/Singleton.h>
#include <render/resource/Texture.h>
#include <render/resource/TextureStreamingPool.h>

#include <unordered_map>

namespace sky {

    class TextureManager : public Singleton<TextureManager> {
    public:
        TextureManager() = default;
        ~TextureManager() override = default;

    private:
        std::unordered_map<Uuid, RDTexturePtr> textureInstance;
    };

} // namespace sky