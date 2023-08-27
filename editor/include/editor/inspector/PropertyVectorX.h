//
// Created by Zach Lee on 2021/12/16.
//

#pragma once
#include <QWidget>
#include <QHBoxLayout>
#include <QDoubleValidator>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include <editor/inspector/PropertyWidget.h>
#include <framework/serialization/SerializationContext.h>

namespace sky {
    struct TypeNode;
} // namespace sky

namespace sky::editor {

    class PropertyBool : public PropertyWidget {
    public:
        explicit PropertyBool(void *inst, const TypeNode *node, QWidget* parent) : PropertyWidget(inst, node, parent)
        {
            layout->setSpacing(0);
            layout->addWidget(label, 0, Qt::AlignLeft | Qt::AlignTop);

            box = new QCheckBox(this);
            layout->addWidget(box, 0, Qt::AlignLeft | Qt::AlignTop);

            connect(box, &QCheckBox::stateChanged, this, [this](int v) {
                *reinterpret_cast<bool *>(instance) = (v == Qt::Checked);
            });
        }

        void Refresh() override
        {
            box->setCheckState(*reinterpret_cast<bool *>(instance) ? Qt::Checked : Qt::Unchecked);
        }

    private:
        QCheckBox* box;
    };

    template <typename T>
    class PropertyScalar : public PropertyWidget {
    public:
        explicit PropertyScalar(void *inst, const TypeNode *node, QWidget* parent) : PropertyWidget(inst, node, parent)
        {
            layout->setSpacing(0);
            layout->addWidget(label);

            line = new QLineEdit(this);
            if constexpr (std::is_floating_point_v<T>) {
                line->setValidator(new QDoubleValidator(this));
            } else if constexpr (std::is_integral_v<T>) {
                line->setValidator(new QIntValidator(std::numeric_limits<T>::min(), std::numeric_limits<T>::max(), this));
            }
            layout->addWidget(line);

            connect(line, &QLineEdit::textEdited, this, [this](const QString &s) {
                auto *ptr = reinterpret_cast<T *>(instance);
                if constexpr (std::is_floating_point_v<T>) {
                    *ptr = static_cast<T>(s.toDouble());
                } else if constexpr (std::is_signed_v<T>) {
                    *ptr = static_cast<T>(s.toInt());
                } else if constexpr (std::is_unsigned_v<T>) {
                    *ptr = static_cast<T>(s.toUInt());
                }
            });
        }

        void Refresh() override
        {
            const T* data = reinterpret_cast<const T*>(instance);
            line->setText(QString::number(*data));
        }

        ~PropertyScalar() override = default;

    private:
        QLineEdit* line;
    };

    template <size_t N>
    class PropertyVec : public PropertyWidget {
    public:
        explicit PropertyVec(QWidget* parent) : PropertyWidget(parent)
        {
            layout->setSpacing(0);
            layout->addWidget(label);
            auto *validator = new QDoubleValidator(this);
            const float *val = reinterpret_cast<float *>(instance);
            for (uint32_t i = 0; i < N; ++i) {
                line[i] = new QLineEdit(this);
                line[i]->setValidator(validator);
                layout->addWidget(line[i]);

                connect(line[i], &QLineEdit::textEdited, this, [i, this](const QString &s) {
                    auto val = static_cast<float>(s.toDouble());
                });
            }
        }

        void Refresh() override
        {
            const auto *val = reinterpret_cast<const float *>(instance);
            for (uint32_t i = 0; i < N; ++i) {
                line[i++]->setText(QString::number(val[i]));
            }
        }

        ~PropertyVec() override = default;

    private:
        QLineEdit* line[N];
    };

}