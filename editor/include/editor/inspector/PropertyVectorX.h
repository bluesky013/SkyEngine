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

    template <typename T>
    class PropertyScalar : public PropertyWidget {
    public:
        PropertyScalar(QWidget* parent) : PropertyWidget(parent)
        {
            auto layout = new QHBoxLayout(this);
            layout->setSpacing(0);
            layout->setMargin(0);
            layout->addWidget(label);

            line = new QLineEdit(this);
            if constexpr (std::is_floating_point_v<T>) {
                line->setValidator(new QDoubleValidator(this));
            } else if constexpr (std::is_integral_v<T>) {
                line->setValidator(new QIntValidator(this));
            }
            layout->addWidget(line);

            connect(line, &QLineEdit::textEdited, this, [this](const QString &s) {
                T val = 0;
                if constexpr (std::is_floating_point_v<T>) {
                    val = static_cast<T>(s.toDouble());
                } else if constexpr (std::is_signed_v<T>) {
                    T val = static_cast<T>(s.toInt());
                } else if constexpr (std::is_unsigned_v<T>) {
                    T val = static_cast<T>(s.toUInt());
                }
                memberNode->setterFn(instance, val);
            });
        }

        void Refresh() override
        {
            Any val = memberNode->getterFn(instance, false);
            T* data = static_cast<T*>(val.Data());
            line->setText(QString::number(*data));
        }

        ~PropertyScalar() = default;

    private:
        QLineEdit* line;
    };

    template <size_t N>
    class PropertyVec : public PropertyWidget {
    public:
        PropertyVec(QWidget* parent) : PropertyWidget(parent)
        {
            auto layout = new QHBoxLayout(this);
            layout->setSpacing(0);
            layout->setMargin(0);

            layout->addWidget(label);
            auto validator = new QDoubleValidator(this);
            for (uint32_t i = 0; i < N; ++i) {
                line[i] = new QLineEdit(this);
                line[i]->setValidator(validator);
                memLabel[i] = new QLabel(this);
                memLabel[i]->setFixedWidth(20);
                layout->addWidget(memLabel[i]);
                layout->addWidget(line[i]);

                connect(line[i], &QLineEdit::textEdited, this, [i, this](const QString &s) {
                    float val = static_cast<float>(s.toDouble());
                    memberNode->setterFn(instance, val);
                });
            }
        }

        void SetInstance(void* instance, const QString& str, const TypeMemberNode& node) override
        {
            PropertyWidget::SetInstance(instance, str, node);
            auto member = GetTypeNode(memberNode->info);
            if (member == nullptr) {
                return;
            }
            uint32_t i = 0;
            for (auto& mem : member->members) {
                memLabel[i++]->setText(mem.first.data());
            }
        }

        void Refresh() override
        {
            Any val = memberNode->getterFn(instance, false);
            auto member = GetTypeNode(val);
            if (member == nullptr) {
                return;
            }
            uint32_t i = 0;
            for (auto& mem : member->members) {
                auto memVal = mem.second.getterFn(val.Data(), false);
                line[i++]->setText(QString::number(*memVal.GetAs<float>()));
            }
        }

        ~PropertyVec() = default;

    private:
        QLineEdit* line[N];
        QLabel* memLabel[N];
    };

}