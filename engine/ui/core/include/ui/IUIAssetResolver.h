//
// Created on 2026/09/19.
//

#pragma once

#include <ui/UIDrawData.h>

#include <string>

namespace sky::ui {

    // Core-side seam: document asset refs (id or path) resolve to handles.
    // Font and sub-document resolution land with ui-text / ui-data (groups 7-8);
    // this milestone only needs texture refs.
    class IUIAssetResolver {
    public:
        IUIAssetResolver() = default;
        virtual ~IUIAssetResolver() = default;

        IUIAssetResolver(const IUIAssetResolver &) = delete;
        IUIAssetResolver &operator=(const IUIAssetResolver &) = delete;

        virtual UITextureId ResolveTexture(const std::string &ref) = 0;
    };

} // namespace sky::ui
