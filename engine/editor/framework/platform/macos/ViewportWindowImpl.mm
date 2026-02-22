//
// Created by Zach Lee on 2025/5/25.
//


#include <editor/framework/ViewportWidget.h>
#include <Cocoa/Cocoa.h>
#import <MetalKit/MetalKit.h>

namespace sky::editor {

    void* ViewportWindow::GetNativeWindow()
    {
        setSurfaceType(QSurface::MetalSurface);
        auto* view = (NSView*)(winId());
        return view;
    }

} // namespace sky::editor

