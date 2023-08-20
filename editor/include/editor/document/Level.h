//
// Created by Zach Lee on 2023/1/15.
//

#pragma once
#include <framework/world/World.h>
#include <render/RenderScene.h>
#include <QString>

namespace sky::editor {

    class Level {
    public:
        Level() = default;
        ~Level();

        void New(const QString &level);
        void Open(const QString &level);

        const WorldPtr &GetWorld() const;

    private:
        void Save();
        void Load();
        void InitRenderScene();
        QString path;
        WorldPtr world;
        RenderScene *renderScene = nullptr;
    };

} // namespace sky::editor
