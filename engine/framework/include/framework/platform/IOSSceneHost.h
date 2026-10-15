//
// Created on 2026/10/15.
//

#pragma once

namespace sky::ios {

    // Called on the UIKit main thread with a UIWindowScene from the application's
    // scene delegate. The host supports one scene; a secondary scene is rejected.
    // Connecting starts inactive. The delegate must explicitly activate it.
    bool ConnectScene(void *windowScene);
    void SetSceneActive(void *windowScene, bool active);
    void DisconnectScene(void *windowScene);

} // namespace sky::ios
