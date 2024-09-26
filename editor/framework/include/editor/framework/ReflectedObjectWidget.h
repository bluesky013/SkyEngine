//
// Created by blues on 2024/7/7.
//

#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QFormLayout>
#include <QDoubleValidator>
#include <QIntValidator>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QColorDialog>
#include <QPalette>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <core/math/MathUtil.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/asset/AssetDataBase.h>

namespace sky::editor {

    class ReflectedMemberWidget;

    class ReflectedObjectWidget : public QWidget {
    public:
        ReflectedObjectWidget(void *obj, const TypeNode *node, QWidget *parent);
        ~ReflectedObjectWidget() override = default;

    protected:
        virtual void Refresh();

        void *object;
        const TypeNode *typeNode = nullptr;

        std::vector<ReflectedMemberWidget*> members;
    };

    class ReflectedMemberWidget : public QWidget {
    public:
        ReflectedMemberWidget(void *obj, const TypeMemberNode *node, QWidget *parent)
            : QWidget(parent)
            , object(obj)
            , memberNode(node)
        {
            setLayout(new QHBoxLayout(this));
            layout()->setAlignment(Qt::AlignLeft);
            layout()->setContentsMargins(0, 0, 0, 0);
        }
        ~ReflectedMemberWidget() override = default;

    protected:
        void *object;
        const TypeMemberNode* memberNode = nullptr;
    };

    template <typename T>
    class PropertyScalar : public ReflectedMemberWidget {
    public:
        explicit PropertyScalar(void *obj, const TypeMemberNode *node, QWidget *parent) : ReflectedMemberWidget(obj, node, parent)
        {
            line = new QLineEdit(this);
            layout()->addWidget(line);

            if constexpr (std::is_floating_point_v<T>) {
                line->setValidator(new QDoubleValidator(this));
            } else if constexpr (std::is_integral_v<T>) {
                line->setValidator(new QIntValidator(std::numeric_limits<T>::min(), std::numeric_limits<T>::max(), this));
            }

            connect(line, &QLineEdit::textEdited, this, [this](const QString &s) {
                T value = 0;
                if constexpr (std::is_floating_point_v<T>) {
                    value = static_cast<T>(s.toDouble());
                } else if constexpr (std::is_signed_v<T>) {
                    value = static_cast<T>(s.toInt());
                } else if constexpr (std::is_unsigned_v<T>) {
                    value = static_cast<T>(s.toUInt());
                }
                memberNode->setterFn(object, &value);
                RefreshValue();
            });

            RefreshValue();
        }

        void RefreshValue()
        {
            auto anyVal = memberNode->getterConstFn(object);
            auto *val = static_cast<float*>(anyVal.Data());
            line->setText(QString::number(*val));
        }

        ~PropertyScalar() override = default;

    private:
        QLineEdit* line;
    };

    template <size_t N>
    class PropertyVec : public ReflectedMemberWidget {
    public:
        explicit PropertyVec(void *obj, const TypeMemberNode *node, QWidget *parent) : ReflectedMemberWidget(obj, node, parent)
        {
            auto *validator = new QDoubleValidator(this);
            for (uint32_t i = 0; i < N; ++i) {
                line[i] = new QLineEdit(this);
                line[i]->setValidator(validator);
                layout()->addWidget(line[i]);
                connect(line[i], &QLineEdit::editingFinished, this, [i, this]() {
                    value[i] = static_cast<float>(line[i]->text().toDouble());
                    memberNode->setterFn(object, value);
                    RefreshValue(i);
                });
            }
            RefreshValue();
        }

        void RefreshValue()
        {
            auto anyVal = memberNode->getterConstFn(object);
            auto *val = static_cast<float*>(anyVal.Data());
            for (uint32_t i = 0; i < N; ++i) {
                value[i] = val[i];
                line[i]->setText(QString::number(val[i]));
            }
        }

        void RefreshValue(int i)
        {
            line[i]->setText(QString::number(value[i]));
        }

        ~PropertyVec() override = default;

    private:
        QLineEdit* line[N];
        float value[N] = {0};
    };

    class PropertyColor : public ReflectedMemberWidget {
    public:
        PropertyColor(void *obj, const TypeMemberNode *node, QWidget *parent) : ReflectedMemberWidget(obj, node, parent)
        {
            button = new QPushButton(this);
            button->setFixedWidth(32);
            layout()->addWidget(button);

            connect(button, &QPushButton::clicked, this, [this](bool v) {
                QColorDialog dialog;
                if (dialog.exec() != 0) {
                    auto qColor = dialog.selectedColor();
                    auto color = Color(static_cast<float>(qColor.redF()),
                                 static_cast<float>(qColor.greenF()),
                                 static_cast<float>(qColor.blueF()),
                                 static_cast<float>(qColor.alphaF())
                    );
                    memberNode->setterFn(object, &color);

                    RefreshValue(qColor);
                }
            });
            RefreshValue();
        }

        void RefreshValue()
        {
            auto anyVal = memberNode->getterConstFn(object);
            auto *color = anyVal.GetAs<Color>();
            QColor qColor;
            qColor.setRedF(color->r);
            qColor.setGreenF(color->g);
            qColor.setBlueF(color->b);
            qColor.setAlphaF(color->a);
            RefreshValue(qColor);
        }

        void RefreshValue(const QColor &qColor)
        {
            if(qColor.isValid()) {
                QString qss = QString("background-color: %1").arg(qColor.name());
                button->setStyleSheet(qss);
            }
        }

        ~PropertyColor() override = default;

    private:
        QPushButton *button = nullptr;
    };

    class AssetLineEditor : public QLineEdit {
    public:
        explicit AssetLineEditor(QWidget *parent) : QLineEdit(parent) {}
        ~AssetLineEditor() override = default;

    private:
        void dropEvent(QDropEvent *event) override;
        void dragEnterEvent(QDragEnterEvent *event) override;
        void dragMoveEvent(QDragMoveEvent *event) override;
    };

    class PropertyUuid : public ReflectedMemberWidget {
    public:
        PropertyUuid(void *obj, const TypeMemberNode *node, QWidget *parent);
        ~PropertyUuid() override = default;

        void RefreshValue();

    private:
        QLineEdit* lineEdit = nullptr;
        std::string_view assetType;
    };

    class PropertyBool : public ReflectedMemberWidget {
    public:
        PropertyBool(void *obj, const TypeMemberNode *node, QWidget *parent) : ReflectedMemberWidget(obj, node, parent)
        {
            box = new QCheckBox(this);
            layout()->addWidget(box);

            connect(box, &QCheckBox::stateChanged, this, [this](int v) {
                bool val = (v == Qt::Checked);
                memberNode->setterFn(object, &val);
            });
            RefreshValue();
        }

        void RefreshValue()
        {
            auto any = memberNode->getterConstFn(object);
            auto *pVal = any.GetAs<bool>();
            box->setCheckState(*pVal ? Qt::Checked : Qt::Unchecked);
        }

        ~PropertyBool() override = default;

    private:
        QCheckBox *box = nullptr;
    };

    class PropertyEnum : public ReflectedMemberWidget {
    public:
        PropertyEnum(void *obj, const TypeMemberNode *node, QWidget *parent) : ReflectedMemberWidget(obj, node, parent)
        {
            box = new QComboBox(this);
            layout()->addWidget(box);

            info = GetTypeNode(node->info);
            for (const auto &[value, key] : info->enums) {
                box->addItem(key.data());
            }

            connect(box, qOverload<int>(&QComboBox::activated), this, [this](int index) {
                auto str = box->itemText(index);
                for (const auto &[key, val] : info->enums) {
                    if (val == std::string_view(str.toStdString())) {
                        memberNode->setterFn(object, &key);
                        break;
                    }
                }

            });
            RefreshValue();
        }
        ~PropertyEnum() override = default;

    private:
        void RefreshValue()
        {
            auto any = memberNode->getterConstFn(object);
            auto *pVal = any.GetAs<uint64_t >();

            SKY_ASSERT(info->enums.count(*pVal));
            box->setCurrentText(info->enums.at(*pVal).data());
        }

        const TypeNode* info = nullptr;
        QComboBox *box = nullptr;
    };

} // namespace sky::editor