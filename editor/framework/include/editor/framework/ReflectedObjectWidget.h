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
#include <QLabel>
#include <core/math/MathUtil.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/serialization/ArrayVisitor.h>

namespace sky::editor {

    class ReflectedMemberWidget;

    class ReflectedObjectWidget : public QWidget {
        Q_OBJECT
    public:
        ReflectedObjectWidget(void *obj, const TypeNode *node, QWidget *parent);
        ~ReflectedObjectWidget() override = default;

    private Q_SLOTS:
        void OnValueChanged();
    Q_SIGNALS:
        void ValueChanged(); // NOLINT

    protected:
        void *object;
        const TypeNode *typeNode = nullptr;

        std::vector<ReflectedMemberWidget*> members;
    };

    template <class T>
    class TObjectWidget : public QWidget {
    public:
        explicit TObjectWidget(QWidget *parent) : QWidget(parent)
        {
            auto *layout = new QVBoxLayout(this);
            setLayout(layout);
            setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            setMinimumWidth(32);

            const auto *info = TypeInfoObj<T>::Get()->RtInfo();
            const auto *typeNode = GetTypeNode(info);
            layout->addWidget(new ReflectedObjectWidget(&value, typeNode, this));
        }

        const T& GetValue() const
        {
            return value;
        }

    private:
        T value;
    };

    class ReflectedMemberWidget : public QWidget {
        Q_OBJECT
    public:
        ReflectedMemberWidget(void *obj, const serialize::TypeMemberNode *node, QWidget *parent)
            : QWidget(parent)
            , object(obj)
            , memberNode(node)
        {
            setLayout(new QHBoxLayout(this));
            layout()->setAlignment(Qt::AlignLeft);
            layout()->setContentsMargins(0, 0, 0, 0);
            layout()->setSpacing(0);

            setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            setMinimumWidth(32);
        }
        ~ReflectedMemberWidget() override = default;

    Q_SIGNALS:
        void ValueChanged(); // NOLINT

    protected:
        void *object;
        const serialize::TypeMemberNode* memberNode = nullptr;
    };

    template <typename T>
    class PropertyScalar : public ReflectedMemberWidget {
    public:
        explicit PropertyScalar(void *obj, const serialize::TypeMemberNode *node, QWidget *parent) : ReflectedMemberWidget(obj, node, parent)
        {
            line = new QLineEdit(this);
            layout()->addWidget(line);

            if constexpr (std::is_floating_point_v<T>) {
                line->setValidator(new QDoubleValidator(this));
                isFloatingType = true;
            } else if constexpr (std::is_integral_v<T>) {
                line->setValidator(new QIntValidator(std::numeric_limits<T>::min(), std::numeric_limits<T>::max(), this));
                isFloatingType = false;
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
                emit ValueChanged();
                RefreshValue();
            });

            RefreshValue();
        }

        void RefreshValue()
        {
            auto anyVal = memberNode->getterConstFn(object);

            if (isFloatingType) {
                auto *val = static_cast<float*>(anyVal.Data());
                line->setText(QString::number(*val));
            } else {
                auto *val = static_cast<uint32_t*>(anyVal.Data());
                line->setText(QString::number(*val));
            }
        }

        ~PropertyScalar() override = default;

    private:
        QLineEdit* line;
        bool isFloatingType = true;
    };

    template <size_t N>
    class PropertyVec : public ReflectedMemberWidget {
    public:
        explicit PropertyVec(void *obj, const serialize::TypeMemberNode *node, QWidget *parent) : ReflectedMemberWidget(obj, node, parent)
        {
            auto *validator = new QDoubleValidator(this);
            for (uint32_t i = 0; i < N; ++i) {
                line[i] = new QLineEdit(this);
                line[i]->setValidator(validator);
                layout()->addWidget(line[i]);
                connect(line[i], &QLineEdit::textEdited, this, [i, this]() {
                    value[i] = static_cast<float>(line[i]->text().toDouble());
                    memberNode->setterFn(object, value);
                    emit ValueChanged();
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
        Q_OBJECT
    public:
        PropertyColor(void *obj, const serialize::TypeMemberNode *node, QWidget *parent) : ReflectedMemberWidget(obj, node, parent)
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
                    emit ValueChanged();
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

    class PropertyColorRGB : public ReflectedMemberWidget {
        Q_OBJECT
    public:
        PropertyColorRGB(void *obj, const serialize::TypeMemberNode *node, QWidget *parent) : ReflectedMemberWidget(obj, node, parent)
        {
            button = new QPushButton(this);
            button->setFixedWidth(32);
            layout()->addWidget(button);

            connect(button, &QPushButton::clicked, this, [this](bool v) {
                QColorDialog dialog;
                if (dialog.exec() != 0) {
                    auto qColor = dialog.selectedColor();
                    auto color = ColorRGB(static_cast<float>(qColor.redF()),
                                        static_cast<float>(qColor.greenF()),
                                        static_cast<float>(qColor.blueF())
                     );
                    memberNode->setterFn(object, &color);
                    emit ValueChanged();
                    RefreshValue(qColor);
                }
            });
            RefreshValue();
        }

        void RefreshValue()
        {
            auto anyVal = memberNode->getterConstFn(object);
            auto *color = anyVal.GetAs<ColorRGB>();
            QColor qColor;
            qColor.setRedF(color->r);
            qColor.setGreenF(color->g);
            qColor.setBlueF(color->b);
            RefreshValue(qColor);
        }

        void RefreshValue(const QColor &qColor)
        {
            if(qColor.isValid()) {
                QString qss = QString("background-color: %1").arg(qColor.name());
                button->setStyleSheet(qss);
            }
        }

        ~PropertyColorRGB() override = default;

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
        Q_OBJECT
    public:
        PropertyUuid(void *obj, const serialize::TypeMemberNode *node, QWidget *parent);
        ~PropertyUuid() override = default;

        void RefreshValue();

    private:
        QLineEdit* lineEdit = nullptr;
        std::string_view assetType;
    };

    class PropertyBool : public ReflectedMemberWidget {
        Q_OBJECT
    public:
        PropertyBool(void *obj, const serialize::TypeMemberNode *node, QWidget *parent) : ReflectedMemberWidget(obj, node, parent)
        {
            box = new QCheckBox(this);
            layout()->addWidget(box);

            connect(box, &QCheckBox::stateChanged, this, [this](int v) {
                bool val = (v == Qt::Checked);
                memberNode->setterFn(object, &val);
                emit ValueChanged();
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
        Q_OBJECT
    public:
        PropertyEnum(void *obj, const serialize::TypeMemberNode *node, QWidget *parent) : ReflectedMemberWidget(obj, node, parent)
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
                        emit ValueChanged();
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

            SKY_ASSERT(info->enums.count(*pVal) != 0)
            box->setCurrentText(info->enums.at(*pVal).data());
        }

        const TypeNode* info = nullptr;
        QComboBox *box = nullptr;
    };

    class PropertySequenceContainerWidget;
    class PropertySequenceItemWidget : public QWidget {
        Q_OBJECT
    public:
        PropertySequenceItemWidget(uint32_t idx, PropertySequenceContainerWidget* container, void *obj, const TypeNode *node, QWidget *parent);

    private Q_SLOTS:
        void OnValueChanged();
    Q_SIGNALS:
        void ValueChanged(); // NOLINT
    private:
        ReflectedObjectWidget* objectWidget = nullptr;
        PropertySequenceContainerWidget* owner = nullptr;
        uint32_t index;
    };

    class PropertySequenceContainerWidget : public ReflectedMemberWidget {
        Q_OBJECT
    public:
        PropertySequenceContainerWidget(void *obj, const serialize::TypeMemberNode *node, QWidget *parent) : ReflectedMemberWidget(obj, node, parent)
        {
            array = new QWidget(this);
            array->setLayout(new QVBoxLayout());

            layout()->addWidget(array);
            auto *addBtn = new QPushButton("+");
            addBtn->setFixedWidth(20);
            connect(addBtn, &QPushButton::clicked, this, [this]() {
                auto any = memberNode->getterFn(object);
                auto *pVal = any.GetAs<SequenceVisitor>();
                pVal->Emplace();
                OnValueChanged();
                RefreshValue();
            });
            layout()->addWidget(addBtn);
            RefreshValue();
        }

        void RefreshValue()
        {
            for (auto &wgt : widgets) {
                disconnect(widgets.back().get(), &PropertySequenceItemWidget::ValueChanged, this, &PropertySequenceContainerWidget::OnValueChanged);
                array->layout()->removeWidget(wgt.get());
            }
            widgets.clear();

            auto any = memberNode->getterFn(object);
            auto *pVal = any.GetAs<SequenceVisitor>();
            auto count = pVal->Count();
            widgets.reserve(count);

            const auto *info = GetTypeNode(pVal->GetValueType());
            SKY_ASSERT(info != nullptr)
            for (uint32_t i = 0; i < count; ++i) {
                auto *ptr = pVal->GetByIndex(i);
                widgets.emplace_back(new PropertySequenceItemWidget(i, this, ptr, info, nullptr));
                array->layout()->addWidget(widgets.back().get());
                connect(widgets.back().get(), &PropertySequenceItemWidget::ValueChanged, this, &PropertySequenceContainerWidget::OnValueChanged);
            }
        }

        void RemoveIndex(uint32_t index)
        {
            auto any = memberNode->getterFn(object);
            auto *pVal = any.GetAs<SequenceVisitor>();
            pVal->Erase(index);
            OnValueChanged();
            RefreshValue();
        }

    private Q_SLOTS:
        void OnValueChanged()
        {
            if (memberNode->valueChangedFn != nullptr) {
                memberNode->valueChangedFn(object);
            }
            emit ValueChanged();
        }

    private: // NOLINT
        QWidget* array = nullptr;
        std::vector<std::unique_ptr<PropertySequenceItemWidget>> widgets;
    };
} // namespace sky::editor