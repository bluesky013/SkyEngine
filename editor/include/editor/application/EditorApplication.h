//
// Created by Zach Lee on 2021/12/12.
//

#pragma once
#include <framework/application/Application.h>
#include <QTimer>
#include <QObject>

namespace sky {
    class SkyEngine;
}

namespace sky::editor {

    class EditorApplication : public QObject, public Application {
        Q_OBJECT
    public:
        EditorApplication();
        ~EditorApplication();

        bool Init(StartInfo &) override;
    private:
        QTimer *timer = nullptr;
        SkyEngine *engine = nullptr;
    };

}
