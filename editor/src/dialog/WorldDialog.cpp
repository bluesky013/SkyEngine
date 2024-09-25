//
// Created by Zach Lee on 2023/1/15.
//

#include "WorldDialog.h"
#include <QVBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QDir>
#include <QPushButton>

namespace sky::editor {

    WorldDialog::WorldDialog()
    {
        setWindowTitle(tr("New World"));

        auto *layout = new QVBoxLayout(this);
        auto *levelText = new QLineEdit(this);
        connect(levelText, &QLineEdit::textEdited, this, [this](const QString name) {
            levelName = name + ".world";
        });

        auto *btn = new QPushButton(tr("New"), this);
        connect(btn, &QPushButton::clicked, this, [this]() {
            accept();
        });

        layout->addWidget(new QLabel(tr("World:"), this));
        layout->addWidget(levelText);
        layout->addWidget(btn);
        setLayout(layout);
    }

    const QString &WorldDialog::LevelName() const
    {
        return levelName;
    }

} // namespace sky::editor