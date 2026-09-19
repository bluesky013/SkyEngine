//
// Created on 2026/09/19.
//

#include <gtest/gtest.h>
#include <ui/data/UIBuilder.h>
#include <ui/data/UIDataContext.h>
#include <ui/data/UIDocument.h>
#include <ui/data/UIElementRegistry.h>
#include <ui/IUIAssetResolver.h>
#include <ui/widgets/Image.h>
#include <ui/widgets/Panel.h>

namespace sky::ui {

    namespace {

        class FakeResolver : public IUIAssetResolver {
        public:
            UITextureId ResolveTexture(const std::string &ref) override
            {
                return ref == "ok" ? 42u : UI_INVALID_TEXTURE;
            }
        };

    } // namespace

    TEST(UIDataTest, UnknownTypeDiagnostic)
    {
        auto registry = UIElementRegistry::CreateDefault();
        const char *json = R"({"root":{"type":"Panel","name":"Root","children":[{"type":"Nope"}]}})";

        auto document = UIDocumentLoader::LoadFromString(json, registry, nullptr);
        ASSERT_TRUE(document->root != nullptr);
        EXPECT_TRUE(document->HasErrors());
        EXPECT_EQ(document->root->GetChildren().size(), 0u);
    }

    TEST(UIDataTest, MissingAssetWarning)
    {
        auto registry = UIElementRegistry::CreateDefault();
        FakeResolver resolver;
        const char *json = R"({"root":{"type":"Image","props":{"visual":{"texture":"missing"}}}})";

        auto document = UIDocumentLoader::LoadFromString(json, registry, &resolver);
        ASSERT_FALSE(document->HasErrors());

        bool found = false;
        for (const auto &diagnostic : document->diagnostics) {
            if (diagnostic.code == "missing-asset") {
                found = true;
            }
        }
        EXPECT_TRUE(found);
    }

    TEST(UIDataTest, NameLookup)
    {
        auto registry = UIElementRegistry::CreateDefault();
        const char *json = R"({"root":{"type":"Panel","name":"Root","children":[{"type":"Image","name":"Pic"}]}})";

        auto document = UIDocumentLoader::LoadFromString(json, registry, nullptr);
        ASSERT_TRUE(document->root != nullptr);
        EXPECT_NE(document->FindByName("Pic"), nullptr);
        EXPECT_EQ(document->FindByName("None"), nullptr);
    }

    TEST(UIDataTest, TextureResolvedAndApplied)
    {
        auto registry = UIElementRegistry::CreateDefault();
        FakeResolver resolver;
        const char *json = R"({"root":{"type":"Image","props":{"visual":{"texture":"ok"}}}})";

        auto document = UIDocumentLoader::LoadFromString(json, registry, &resolver);
        ASSERT_TRUE(document->root != nullptr);

        auto *image = static_cast<Image *>(document->root.get());
        EXPECT_EQ(image->GetTexture(), 42u);
    }

    TEST(UIDataTest, BindingUpdateAndSkip)
    {
        auto registry = UIElementRegistry::CreateDefault();
        const char *json = R"({"root":{"type":"Image","bindings":[{"target":"visual.texture","source":"tex"}]}})";

        auto document = UIDocumentLoader::LoadFromString(json, registry, nullptr);
        ASSERT_TRUE(document->root != nullptr);
        auto *image = static_cast<Image *>(document->root.get());

        UIDataContext context;
        context.SetValue("tex", UIPropertyValue::Int(9));

        EXPECT_TRUE(document->ApplyBindings(context));
        EXPECT_EQ(image->GetTexture(), 9u);
        EXPECT_FALSE(document->ApplyBindings(context));
    }

    TEST(UIDataTest, ConverterRemap)
    {
        auto registry = UIElementRegistry::CreateDefault();
        const char *json = R"({"root":{"type":"Image","bindings":[{"target":"visual.tint","source":"v","converter":"remap","args":{"in":[0,1],"out":[0,255]}}]}})";

        auto document = UIDocumentLoader::LoadFromString(json, registry, nullptr);
        ASSERT_TRUE(document->root != nullptr);
        auto *image = static_cast<Image *>(document->root.get());

        UIDataContext context;
        context.SetValue("v", UIPropertyValue::Float(1.0f));
        document->ApplyBindings(context);

        EXPECT_EQ(image->GetTint(), 255u);
    }

    TEST(UIDataTest, BuilderLoaderParity)
    {
        auto registry = UIElementRegistry::CreateDefault();

        const char *json = R"({"root":{"type":"Panel","name":"Root","children":[{"type":"Image"},{"type":"Button"}]}})";
        auto document = UIDocumentLoader::LoadFromString(json, registry, nullptr);
        ASSERT_TRUE(document->root != nullptr);

        UIBuilder builder(registry);
        UIElement *root = builder.SetRoot("Panel");
        root->SetName("Root");
        builder.AddChild(root, "Image");
        builder.AddChild(root, "Button");
        auto built = builder.Build();

        ASSERT_TRUE(built != nullptr);
        EXPECT_EQ(built->GetChildren().size(), document->root->GetChildren().size());
        EXPECT_EQ(built->GetChildren().size(), 2u);
    }

} // namespace sky::ui
