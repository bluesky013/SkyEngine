//
// Created by Zach Lee on 2023/1/15.
//

#include "LevelDialog.h"
#include <QVBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QDir>
#include <QPushButton>

namespace sky::editor {

    LevelDialog::LevelDialog()
    {
        setWindowTitle(tr("New Level"));
        setSizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);

        auto layout = new QVBoxLayout(this);
        auto levelText = new QLineEdit(this);
        connect(levelText, &QLineEdit::textEdited, this, [this](const QString name) {
            levelName = name + ".level";
        });

        auto btn = new QPushButton(tr("New"), this);
        connect(btn, &QPushButton::clicked, this, [this]() {
            accept();
        });

        layout->addWidget(new QLabel(tr("Level:"), this));
        layout->addWidget(levelText);
        layout->addWidget(btn);
        setLayout(layout);
    }

    const QString &LevelDialog::LevelName() const
    {
        return levelName;
    }

} // namespace sky::editor