//
// Created by Zach Lee on 2021/12/14.
//

#pragma once
#include <unordered_map>
#include <framework/environment/Singleton.h>

class QDockWidget;

namespace sky::editor {

    enum class DockId : uint32_t {
        WORLD,
        INSPECTOR,
        BROWSER
    };

    class DockManager : public Singleton<DockManager> {
    public:
        void Register(uint32_t key, QDockWidget& dock);
        void UnRegister(uint32_t key);

    private:
        friend class Singleton<DockManager>;

        DockManager() = default;
        ~DockManager() = default;

        std::unordered_map<uint32_t, QDockWidget*> docks;
    };

}