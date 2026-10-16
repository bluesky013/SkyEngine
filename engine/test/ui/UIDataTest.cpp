//
// Created on 2026/09/19.
//

#include <gtest/gtest.h>
#include <ui/data/UIBuilder.h>
#include <ui/data/UIDataContext.h>
#include <ui/data/UIDocument.h>
#include <ui/data/UIElementRegistry.h>
#include <ui/IUIAssetResolver.h>
#include <ui/UIPaintContext.h>
#include <ui/text/UIBuiltinFontProvider.h>
#include <ui/text/UITextSystem.h>
#include <ui/widgets/Image.h>
#include <ui/widgets/Panel.h>
#include <ui/widgets/Text.h>

#include <unordered_map>

namespace sky::ui {

    namespace {

        class FakeResolver : public IUIAssetResolver {
        public:
            UITextureId ResolveTexture(const std::string &ref) override
            {
                return ref == "ok" ? 42u : UI_INVALID_TEXTURE;
            }
        };

        class PropertyProbe : public UIElement {
        public:
            bool SetProperty(const std::string &path, const UIPropertyValue &value) override
            {
                properties[path] = value;
                ++calls;
                return true;
            }

            std::unordered_map<std::string, UIPropertyValue> properties;
            size_t calls = 0;
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

    TEST(UIDataTest, NestedScalarPropertiesUseRegisteredSettersOnce)
    {
        auto registry = UIElementRegistry::CreateDefault();
        registry.Register("PropertyProbe", []() { return std::make_unique<PropertyProbe>(); });
        const char *json = R"({"root":{"type":"PropertyProbe","props":{
            "custom":{"enabled":true,"nested":{"count":4294967297,"step":0.25,"label":"hello"}},
            "ignored":{"array":[1,2],"empty":null},
            "layout":{"size":[20,10],"unknown":7},
            "visual":{"tint":"#12345678"},"button":{"interactable":false}
        }}})";

        auto document = UIDocumentLoader::LoadFromString(json, registry, nullptr);
        ASSERT_NE(document->root, nullptr);
        EXPECT_TRUE(document->diagnostics.empty());
        const auto *probe = static_cast<const PropertyProbe *>(document->root.get());
        ASSERT_EQ(probe->properties.size(), 6u);
        EXPECT_EQ(probe->calls, 6u);
        ASSERT_EQ(probe->properties.count("custom.enabled"), 1u);
        EXPECT_EQ(probe->properties.at("custom.enabled").type, UIPropertyValue::Type::BOOL);
        EXPECT_TRUE(probe->properties.at("custom.enabled").boolValue);
        ASSERT_EQ(probe->properties.count("custom.nested.count"), 1u);
        EXPECT_EQ(probe->properties.at("custom.nested.count").type, UIPropertyValue::Type::INT);
        EXPECT_EQ(probe->properties.at("custom.nested.count").intValue, 4294967297LL);
        ASSERT_EQ(probe->properties.count("custom.nested.step"), 1u);
        EXPECT_EQ(probe->properties.at("custom.nested.step").type, UIPropertyValue::Type::FLOAT);
        EXPECT_FLOAT_EQ(probe->properties.at("custom.nested.step").floatValue, 0.25f);
        ASSERT_EQ(probe->properties.count("custom.nested.label"), 1u);
        EXPECT_EQ(probe->properties.at("custom.nested.label").type, UIPropertyValue::Type::STRING);
        EXPECT_EQ(probe->properties.at("custom.nested.label").stringValue, "hello");
        ASSERT_EQ(probe->properties.count("visual.tint"), 1u);
        EXPECT_EQ(probe->properties.at("visual.tint").intValue, 0x12345678);
        ASSERT_EQ(probe->properties.count("button.interactable"), 1u);
        EXPECT_FALSE(probe->properties.at("button.interactable").boolValue);
        EXPECT_FLOAT_EQ(probe->GetLayout().width.value, 20.0f);
        EXPECT_FLOAT_EQ(probe->GetLayout().height.value, 10.0f);
    }

    TEST(UIDataTest, JsonTextPropertiesMatchCppMeasurementAndPaint)
    {
        auto registry = UIElementRegistry::CreateDefault();
        const char *json = R"({"root":{"type":"Text","props":{"text":{"content":"ABCD","size":10,"wrap":true}}}})";
        auto document = UIDocumentLoader::LoadFromString(json, registry, nullptr);
        ASSERT_NE(document->root, nullptr);
        EXPECT_TRUE(document->diagnostics.empty());
        auto *loaded = static_cast<Text *>(document->root.get());

        UIBuiltinFontProvider provider;
        UITextSystem textSystem(&provider, nullptr);
        loaded->SetTextSystem(&textSystem);
        Text built;
        built.SetTextSystem(&textSystem);
        ASSERT_TRUE(built.SetProperty("text.content", UIPropertyValue::String("ABCD")));
        ASSERT_TRUE(built.SetProperty("text.size", UIPropertyValue::Int(10)));
        ASSERT_TRUE(built.SetProperty("text.wrap", UIPropertyValue::Bool(true)));
        EXPECT_EQ(loaded->GetContent(), built.GetContent());
        EXPECT_EQ(loaded->GetFontSize(), built.GetFontSize());
        EXPECT_TRUE(loaded->GetWrap());

        const UIConstraints budget{{}, {12.0f, 100.0f}};
        const UISize loadedSize = loaded->MeasureDesired(budget);
        const UISize builtSize = built.MeasureDesired(budget);
        EXPECT_FLOAT_EQ(loadedSize.width, 12.0f);
        EXPECT_FLOAT_EQ(loadedSize.height, 20.0f);
        EXPECT_FLOAT_EQ(loadedSize.width, builtSize.width);
        EXPECT_FLOAT_EQ(loadedSize.height, builtSize.height);
        loaded->Arrange({0.0f, 0.0f, loadedSize.width, loadedSize.height});
        built.Arrange({0.0f, 0.0f, builtSize.width, builtSize.height});

        UIPaintContext loadedPaint;
        loadedPaint.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        loaded->Paint(loadedPaint);
        UIPaintContext builtPaint;
        builtPaint.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        built.Paint(builtPaint);
        const UIDrawData &loadedDraw = loadedPaint.GetDrawData();
        const UIDrawData &builtDraw = builtPaint.GetDrawData();
        ASSERT_EQ(loadedDraw.vertices.size(), 16u);
        ASSERT_EQ(loadedDraw.vertices.size(), builtDraw.vertices.size());
        EXPECT_EQ(loadedDraw.indices, builtDraw.indices);
        EXPECT_FLOAT_EQ(loadedDraw.vertices[0].y, 0.0f);
        EXPECT_FLOAT_EQ(loadedDraw.vertices[8].y, 10.0f);
        for (size_t index = 0; index < loadedDraw.vertices.size(); ++index) {
            EXPECT_FLOAT_EQ(loadedDraw.vertices[index].x, builtDraw.vertices[index].x);
            EXPECT_FLOAT_EQ(loadedDraw.vertices[index].y, builtDraw.vertices[index].y);
        }
    }

    TEST(UIDataTest, OmittedTextWrapAndUnknownPropertiesRemainHarmless)
    {
        auto registry = UIElementRegistry::CreateDefault();
        const char *json = R"({"root":{"type":"Text","props":{
            "text":{"content":"ABCD","size":10,"unknown":"ignored","array":[1,2],"null":null},
            "unknown":{"nested":{"flag":true,"number":1.5}}
        }}})";
        auto document = UIDocumentLoader::LoadFromString(json, registry, nullptr);
        ASSERT_NE(document->root, nullptr);
        EXPECT_TRUE(document->diagnostics.empty());
        auto *loaded = static_cast<Text *>(document->root.get());
        EXPECT_EQ(loaded->GetContent(), "ABCD");
        EXPECT_EQ(loaded->GetFontSize(), 10u);
        EXPECT_FALSE(loaded->GetWrap());
        UIBuiltinFontProvider provider;
        UITextSystem textSystem(&provider, nullptr);
        loaded->SetTextSystem(&textSystem);
        EXPECT_FLOAT_EQ(loaded->MeasureDesired({{}, {12.0f, 100.0f}}).height, 10.0f);
    }

} // namespace sky::ui
