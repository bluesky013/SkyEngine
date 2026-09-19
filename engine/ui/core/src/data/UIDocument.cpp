//
// Created on 2026/09/19.
//

#include <ui/data/UIDocument.h>
#include <ui/data/IUIDataProvider.h>
#include <ui/data/UIElementRegistry.h>
#include <ui/IUIAssetResolver.h>
#include <ui/UILayout.h>

#include <rapidjson/document.h>

#include <string>

namespace sky::ui {

    namespace {

        void AddDiag(UIDocument &document,
                     UIDiagnosticSeverity severity,
                     const std::string &path,
                     const std::string &code,
                     const std::string &message)
        {
            document.diagnostics.push_back({severity, path, code, message});
        }

        uint32_t ParseHexColor(const char *text)
        {
            std::string value = text != nullptr ? text : "";
            if (!value.empty() && value[0] == '#') {
                value = value.substr(1);
            }
            try {
                return static_cast<uint32_t>(std::stoul(value, nullptr, 16));
            } catch (...) {
                return 0;
            }
        }

        void ApplyLayout(UIElement &element, const rapidjson::Value &layout)
        {
            UILayoutParams params;
            if (layout.HasMember("anchor") && layout["anchor"].IsArray() && layout["anchor"].Size() == 4) {
                const auto &a = layout["anchor"];
                params.anchorMinX = a[0].GetFloat();
                params.anchorMinY = a[1].GetFloat();
                params.anchorMaxX = a[2].GetFloat();
                params.anchorMaxY = a[3].GetFloat();
            }
            if (layout.HasMember("offset") && layout["offset"].IsArray() && layout["offset"].Size() == 4) {
                const auto &o = layout["offset"];
                params.offsetLeft = o[0].GetFloat();
                params.offsetTop = o[1].GetFloat();
                params.offsetRight = o[2].GetFloat();
                params.offsetBottom = o[3].GetFloat();
            }
            if (layout.HasMember("padding") && layout["padding"].IsArray() && layout["padding"].Size() == 4) {
                const auto &p = layout["padding"];
                params.paddingLeft = p[0].GetFloat();
                params.paddingTop = p[1].GetFloat();
                params.paddingRight = p[2].GetFloat();
                params.paddingBottom = p[3].GetFloat();
            }
            auto parseSize = [](const rapidjson::Value &value, UISizeRule &rule) {
                if (value.IsString()) {
                    const std::string mode = value.GetString();
                    if (mode == "fill") {
                        rule.mode = UISizeMode::FILL;
                    } else if (!mode.empty() && mode.back() == '%') {
                        rule.mode = UISizeMode::PERCENT;
                        rule.value = std::stof(mode.substr(0, mode.size() - 1)) / 100.0f;
                    } else {
                        rule.mode = UISizeMode::AUTO;
                    }
                } else if (value.IsNumber()) {
                    rule.mode = UISizeMode::FIXED;
                    rule.value = value.GetFloat();
                }
            };
            if (layout.HasMember("size") && layout["size"].IsArray() && layout["size"].Size() == 2) {
                parseSize(layout["size"][0], params.width);
                parseSize(layout["size"][1], params.height);
            }
            if (layout.HasMember("z") && layout["z"].IsNumber()) {
                params.z = layout["z"].GetFloat();
            }
            element.SetLayout(params);
        }

        UIElementPtr ParseNode(const rapidjson::Value &node,
                               const std::string &path,
                               const UIElementRegistry &registry,
                               IUIAssetResolver *resolver,
                               UIDocument &document)
        {
            if (!node.IsObject() || !node.HasMember("type") || !node["type"].IsString()) {
                AddDiag(document, UIDiagnosticSeverity::ERROR, path, "missing-type", "node has no type");
                return nullptr;
            }

            const std::string type = node["type"].GetString();
            UIElementPtr element = registry.Create(type);
            if (element == nullptr) {
                AddDiag(document, UIDiagnosticSeverity::ERROR, path + "/" + type, "unknown-type", "unknown element type: " + type);
                return nullptr;
            }

            const std::string nodePath = path + "/" + type;

            if (node.HasMember("name") && node["name"].IsString()) {
                element->SetProperty("name", UIPropertyValue::String(node["name"].GetString()));
            }
            if (node.HasMember("visible") && node["visible"].IsBool()) {
                element->SetProperty("visible", UIPropertyValue::Bool(node["visible"].GetBool()));
            }
            if (node.HasMember("enabled") && node["enabled"].IsBool()) {
                element->SetProperty("enabled", UIPropertyValue::Bool(node["enabled"].GetBool()));
            }
            if (node.HasMember("style")) {
                const auto &style = node["style"];
                if (style.IsString()) {
                    element->AddStyleClass(style.GetString());
                } else if (style.IsArray()) {
                    for (const auto &entry : style.GetArray()) {
                        if (entry.IsString()) {
                            element->AddStyleClass(entry.GetString());
                        }
                    }
                }
            }

            if (node.HasMember("props") && node["props"].IsObject()) {
                const auto &props = node["props"];
                if (props.HasMember("layout") && props["layout"].IsObject()) {
                    ApplyLayout(*element, props["layout"]);
                }
                if (props.HasMember("visual") && props["visual"].IsObject()) {
                    const auto &visual = props["visual"];
                    if (visual.HasMember("texture") && visual["texture"].IsString()) {
                        const UITextureId texture = resolver != nullptr ? resolver->ResolveTexture(visual["texture"].GetString()) : UI_INVALID_TEXTURE;
                        if (texture == UI_INVALID_TEXTURE) {
                            AddDiag(document, UIDiagnosticSeverity::WARNING, nodePath, "missing-asset",
                                    std::string("unresolved texture: ") + visual["texture"].GetString());
                        } else {
                            element->SetProperty("visual.texture", UIPropertyValue::Int(texture));
                        }
                    }
                    if (visual.HasMember("tint") && visual["tint"].IsString()) {
                        element->SetProperty("visual.tint", UIPropertyValue::Int(ParseHexColor(visual["tint"].GetString())));
                    }
                }
                if (props.HasMember("button") && props["button"].IsObject()) {
                    const auto &button = props["button"];
                    if (button.HasMember("interactable") && button["interactable"].IsBool()) {
                        element->SetProperty("button.interactable", UIPropertyValue::Bool(button["interactable"].GetBool()));
                    }
                }
            }

            if (node.HasMember("bindings") && node["bindings"].IsArray()) {
                for (const auto &entry : node["bindings"].GetArray()) {
                    if (!entry.IsObject() || !entry.HasMember("target") || !entry.HasMember("source")) {
                        continue;
                    }
                    UIBinding binding;
                    binding.target = element.get();
                    binding.targetPath = entry["target"].GetString();
                    binding.sourcePath = entry["source"].GetString();
                    if (entry.HasMember("converter") && entry["converter"].IsString()) {
                        binding.converter = entry["converter"].GetString();
                    }
                    if (entry.HasMember("args") && entry["args"].IsObject()) {
                        const auto &args = entry["args"];
                        auto readRange = [&args](const char *key, float &low, float &high) {
                            if (args.HasMember(key) && args[key].IsArray() && args[key].Size() == 2) {
                                low = args[key][0].GetFloat();
                                high = args[key][1].GetFloat();
                            }
                        };
                        readRange("in", binding.remapIn0, binding.remapIn1);
                        readRange("out", binding.remapOut0, binding.remapOut1);
                    }
                    document.bindings.push_back(binding);
                }
            }

            if (node.HasMember("children") && node["children"].IsArray()) {
                for (const auto &child : node["children"].GetArray()) {
                    UIElementPtr childElement = ParseNode(child, nodePath, registry, resolver, document);
                    if (childElement != nullptr) {
                        element->AddChild(std::move(childElement));
                    }
                }
            }

            return element;
        }

    } // namespace

    bool UIBinding::Apply(const IUIDataProvider &provider) const
    {
        if (target == nullptr) {
            return false;
        }

        UIPropertyValue source;
        if (!provider.GetValue(sourcePath, source)) {
            return false;
        }

        UIPropertyValue value = source;
        if (converter == "remap") {
            const float input = source.type == UIPropertyValue::Type::INT ? static_cast<float>(source.intValue) : source.floatValue;
            const float span = remapIn1 - remapIn0;
            const float t = span != 0.0f ? (input - remapIn0) / span : 0.0f;
            value = UIPropertyValue::Float(remapOut0 + t * (remapOut1 - remapOut0));
        } else if (converter == "format") {
            const float input = source.type == UIPropertyValue::Type::INT ? static_cast<float>(source.intValue) : source.floatValue;
            value = UIPropertyValue::String(std::to_string(input));
        } else if (converter == "boolToVisible") {
            value = UIPropertyValue::Bool(source.boolValue);
        }

        return target->SetProperty(targetPath, value);
    }

    void UIDocument::Index()
    {
        nameIndex.clear();
        if (root != nullptr) {
            IndexInto(root.get());
        }
    }

    void UIDocument::IndexInto(UIElement *element)
    {
        if (element == nullptr) {
            return;
        }
        if (!element->GetName().empty()) {
            nameIndex[element->GetName()] = element;
        }
        for (const auto &child : element->GetChildren()) {
            IndexInto(child.get());
        }
    }

    UIElement *UIDocument::FindByName(const std::string &name) const
    {
        const auto it = nameIndex.find(name);
        return it != nameIndex.end() ? it->second : nullptr;
    }

    bool UIDocument::HasErrors() const
    {
        for (const auto &diagnostic : diagnostics) {
            if (diagnostic.severity == UIDiagnosticSeverity::ERROR) {
                return true;
            }
        }
        return false;
    }

    bool UIDocument::ApplyBindings(const IUIDataProvider &provider)
    {
        const uint32_t version = provider.GetVersion();
        if (version != 0 && hasVersion && version == lastVersion) {
            return false;
        }

        for (const auto &binding : bindings) {
            binding.Apply(provider);
        }

        lastVersion = version;
        hasVersion = true;
        return true;
    }

    std::unique_ptr<UIDocument> UIDocumentLoader::LoadFromString(const std::string &json,
                                                                 const UIElementRegistry &registry,
                                                                 IUIAssetResolver *resolver)
    {
        auto document = std::make_unique<UIDocument>();

        rapidjson::Document parsed;
        parsed.Parse(json.c_str());
        if (parsed.HasParseError()) {
            AddDiag(*document, UIDiagnosticSeverity::ERROR, "", "parse-error", "invalid json document");
            return document;
        }
        if (!parsed.IsObject() || !parsed.HasMember("root") || !parsed["root"].IsObject()) {
            AddDiag(*document, UIDiagnosticSeverity::ERROR, "", "missing-root", "document has no root node");
            return document;
        }

        document->root = ParseNode(parsed["root"], "", registry, resolver, *document);
        document->Index();
        return document;
    }

} // namespace sky::ui
