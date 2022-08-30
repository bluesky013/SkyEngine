//
// Created by Zach Lee on 2022/8/19.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/util/Macros.h>
#include <render/imgui/GuiRenderer.h>
#include <render/resources/Texture.h>

namespace sky {

    class GuiManager : public Singleton<GuiManager> {
    public:
        SKY_DISABLE_COPY(GuiManager)

        void Init();

        const RDTexturePtr &GetFontTexture() const;

    private:
        friend class Singleton<GuiManager>;
        GuiManager()  = default;
        ~GuiManager() = default;

        RDImagePtr   fontImage;
        RDTexturePtr fontTexture;
    };
} // namespace sky