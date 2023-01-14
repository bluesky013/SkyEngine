//
// Created by Zach Lee on 2023/1/15.
//

#include "LevelDialog.h"

namespace sky::editor {

    LevelDialog::LevelDialog()
    {

    }

    const QString &LevelDialog::LevelPath() const
    {
        return levelPath;
    }

    const QString &LevelDialog::LevelName() const
    {
        return levelName;
    }

} // namespace sky::editor