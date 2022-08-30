//
// Created by Zach Lee on 2022/8/19.
//

#include <imgui.h>
#include <render/imgui/GuiManager.h>

namespace sky {

    void GuiManager::Init()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO       &io = ImGui::GetIO();
        unsigned char *pixels;
        int            width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        VkDeviceSize fontImageSize = width * height * 4 * sizeof(char);

        Image::Descriptor imageDesc = {};
        imageDesc.extent.width      = static_cast<uint32_t>(width);
        imageDesc.extent.height     = static_cast<uint32_t>(height);
        imageDesc.format            = VK_FORMAT_R8G8B8A8_UNORM;
        fontImage                   = std::make_shared<Image>(imageDesc);
        fontImage->InitRHI();
        fontImage->Update(pixels, fontImageSize);

        Texture::Descriptor texDesc = {};
        fontTexture                 = Texture::CreateFromImage(fontImage, texDesc);

        io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(fontImage->GetRHIImage()->GetNativeHandle()));

        io.KeyMap[ImGuiKey_Tab]         = KeyButton::KEY_TAB;
        io.KeyMap[ImGuiKey_LeftArrow]   = KeyButton::KEY_LEFT;
        io.KeyMap[ImGuiKey_RightArrow]  = KeyButton::KEY_RIGHT;
        io.KeyMap[ImGuiKey_UpArrow]     = KeyButton::KEY_UP;
        io.KeyMap[ImGuiKey_DownArrow]   = KeyButton::KEY_DOWN;
        io.KeyMap[ImGuiKey_PageUp]      = KeyButton::KEY_PAGEUP;
        io.KeyMap[ImGuiKey_PageDown]    = KeyButton::KEY_PAGEDOWN;
        io.KeyMap[ImGuiKey_Home]        = KeyButton::KEY_HOME;
        io.KeyMap[ImGuiKey_End]         = KeyButton::KEY_END;
        io.KeyMap[ImGuiKey_Insert]      = KeyButton::KEY_INSERT;
        io.KeyMap[ImGuiKey_Delete]      = KeyButton::KEY_DELETE;
        io.KeyMap[ImGuiKey_Backspace]   = KeyButton::KEY_BACKSPACE;
        io.KeyMap[ImGuiKey_Space]       = KeyButton::KEY_SPACE;
        io.KeyMap[ImGuiKey_Enter]       = KeyButton::KEY_RETURN;
        io.KeyMap[ImGuiKey_Escape]      = KeyButton::KEY_ESCAPE;
        io.KeyMap[ImGuiKey_KeyPadEnter] = KeyButton::KEY_KP_ENTER;
        io.KeyMap[ImGuiKey_A]           = KeyButton::KEY_A;
        io.KeyMap[ImGuiKey_C]           = KeyButton::KEY_C;
        io.KeyMap[ImGuiKey_V]           = KeyButton::KEY_V;
        io.KeyMap[ImGuiKey_X]           = KeyButton::KEY_X;
        io.KeyMap[ImGuiKey_Y]           = KeyButton::KEY_Y;
        io.KeyMap[ImGuiKey_Z]           = KeyButton::KEY_Z;
    }

    const RDTexturePtr &GuiManager::GetFontTexture() const
    {
        return fontTexture;
    }
} // namespace sky