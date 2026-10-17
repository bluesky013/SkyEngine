//
// Created on 2026/09/19.
//

#include <gtest/gtest.h>
#include <ui/UIContext.h>
#include <ui/localization/UILocalization.h>
#include <ui/widgets/Text.h>

namespace sky::ui {

    TEST(UISystemTest, DpiScaleAppliesToContent)
    {
        UIContext context;
        context.SetContentSize(100.0f, 100.0f);
        EXPECT_FLOAT_EQ(context.GetRoot()->GetBounds().Width(), 100.0f);

        context.SetDpiScale(2.0f);
        EXPECT_FLOAT_EQ(context.GetRoot()->GetBounds().Width(), 200.0f);
        EXPECT_FLOAT_EQ(context.GetRoot()->GetBounds().Height(), 200.0f);

        context.SetDpiScale(1.0f);
        EXPECT_FLOAT_EQ(context.GetRoot()->GetBounds().Width(), 100.0f);
    }

    TEST(UISystemTest, TranslateAndFallback)
    {
        auto *localization = UILocalization::Get();
        localization->AddString("en", "greet", "HELLO");
        localization->AddString("zh", "greet", "HELLO_ZH");

        localization->SetLocale("en");
        EXPECT_EQ(localization->Translate("greet"), "HELLO");

        localization->SetLocale("zh");
        EXPECT_EQ(localization->Translate("greet"), "HELLO_ZH");

        EXPECT_EQ(localization->Translate("missing"), "missing");
    }

    TEST(UISystemTest, TextResolvesKey)
    {
        auto *localization = UILocalization::Get();
        localization->AddString("en", "title", "TITLE_EN");
        localization->AddString("fr", "title", "TITLE_FR");

        Text text;
        text.SetTextKey("title");

        localization->SetLocale("en");
        EXPECT_EQ(text.GetResolvedText(), "TITLE_EN");

        localization->SetLocale("fr");
        EXPECT_EQ(text.GetResolvedText(), "TITLE_FR");
    }

} // namespace sky::ui
