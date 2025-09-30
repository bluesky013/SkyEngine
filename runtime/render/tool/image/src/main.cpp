//
// Created by Zach Lee on 2025/9/29.
//

#include <framework/window/NativeWindow.h>
#include <framework/platform/PlatformBase.h>
#include <ImageTool.h>

using namespace sky;

int main(int argc, const char * argv[])
{
    bool exit = false;

    Platform::Get()->Init({});

    NativeWindow::Descriptor winDesc = {};
    winDesc.width = 1366;
    winDesc.height = 768;
    std::unique_ptr<NativeWindow> window;
    window.reset(NativeWindow::Create(winDesc));

    std::unique_ptr<ImageTool> imageTool = std::make_unique<ImageTool>();
    imageTool->Init(window.get());

    while (!exit) {
        Platform::Get()->PoolEvent(exit);

        uint64_t        frequency      = Platform::Get()->GetPerformanceFrequency();
        uint64_t        currentCounter = Platform::Get()->GetPerformanceCounter();
        static uint64_t current        = 0;
        float           delta = current > 0 ? static_cast<float>(static_cast<float>(currentCounter - current) / static_cast<double>(frequency)) : 1.0f / 60.0f;
        current               = currentCounter;

        imageTool->Tick(delta);
    }

    imageTool->Shutdown();
    Platform::Get()->Shutdown();

    return 0;
}