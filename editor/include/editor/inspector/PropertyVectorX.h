//
// Created by Zach Lee on 2021/12/16.
//

#pragma once
#include <QWidget>
#include <QHBoxLayout>
#include <QDoubleValidator>
#include <QLineEdit>
#include <QLabel>
#include <editor/inspector/PropertyWidget.h>
#include <framework/serialization/SerializationContext.h>


namespace sky::editor {

    static const char* LABEL[4] = { "x", "y", "z", "w" };

    template <size_t N>
    class PropertyVec : public PropertyWidget {
    public:
        PropertyVec(QWidget* parent) : PropertyWidget(parent)
        {
            auto layout = new QHBoxLayout(this);
            layout->addWidget(label);
            auto validator = new QDoubleValidator(this);
            for (uint32_t i = 0; i < N; ++i) {
                line[i] = new QLineEdit(this);
                line[i]->setValidator(validator);
                if (N > 1) {
                    layout->addWidget(new QLabel(LABEL[i], this));
                }
                layout->addWidget(line[i]);

                connect(line[i], &QLineEdit::textEdited, this, [i, this](const QString &s) {
                    float val = static_cast<float>(s.toDouble());
                    memberNode->setterFn(instance, val);
                });
            }
        }

        void Refresh() override
        {
            Any val = memberNode->getterFn(instance, false);
            float* data = static_cast<float*>(val.Data());
            for (uint32_t i = 0; i < N; ++i) {
                line[i]->setText(QString::number(data[i]));
            }
        }

        ~PropertyVec() = default;

    private:
        QLineEdit* line[N];
    };

}