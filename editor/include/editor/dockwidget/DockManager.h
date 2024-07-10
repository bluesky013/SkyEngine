//
// Created by Zach Lee on 2021/12/14.
//

#pragma once
#include <unordered_map>
#include <core/environment/Singleton.h>

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
        QDockWidget* Find(uint32_t) const;

    private:
        friend class Singleton<DockManager>;

        DockManager() = default;
        ~DockManager() override = default;

        std::unordered_map<uint32_t, QDockWidget*> docks;
    };

}