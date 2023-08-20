//
// Created by Zach Lee on 2021/12/13.
//

#pragma once

#include <QWindow>
#include <render/RenderWindow.h>
#include <core/environment/Singleton.h>

namespace sky::editor {

    enum class ViewportID : uint32_t {
        EDITOR_PREVIEW
    };

    class Viewport : public QWindow {
        Q_OBJECT
    public:
        Viewport();
        ~Viewport() override;

        void Init(ViewportID id);
        bool event(QEvent *event) override;

        RenderWindow *GetRenderWindow() const { return window; }

    private:
        ViewportID viewportId = ViewportID::EDITOR_PREVIEW;
        RenderWindow *window = nullptr;
    };

    class ViewportManager : public Singleton<ViewportManager> {
    public:
        void Register(ViewportID key, Viewport* vp);
        void UnRegister(ViewportID key);
        Viewport *FindViewport(ViewportID key) const;

    private:
        friend class Singleton<ViewportManager>;

        ViewportManager() = default;
        ~ViewportManager() = default;

        std::unordered_map<ViewportID, Viewport*> viewports;
    };
}
