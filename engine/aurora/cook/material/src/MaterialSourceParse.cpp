//
// Material-source JSON parsing (see MaterialSourceParse.h).
//

#include <aurora/cook/material/MaterialSourceParse.h>

#include <core/logger/Logger.h>

#include <algorithm>
#include <cctype>
#include <cstring>

static const char *TAG = "MaterialSourceParse";

namespace sky::aurora::cook {

    namespace {

        std::string Lower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        }

        template <typename T>
        void WritePOD(std::vector<uint8_t> &out, const T &value)
        {
            out.resize(sizeof(T));
            std::memcpy(out.data(), &value, sizeof(T));
        }

        bool CompareOpFromName(const std::string &name, CompareOp &out)
        {
            const std::string n = Lower(name);
            if (n == "never") {
                out = CompareOp::NEVER;
                return true;
            }
            if (n == "less") {
                out = CompareOp::LESS;
                return true;
            }
            if (n == "equal") {
                out = CompareOp::EQUAL;
                return true;
            }
            if (n == "less_equal" || n == "less_or_equal") {
                out = CompareOp::LESS_OR_EQUAL;
                return true;
            }
            if (n == "greater") {
                out = CompareOp::GREATER;
                return true;
            }
            if (n == "not_equal") {
                out = CompareOp::NOT_EQUAL;
                return true;
            }
            if (n == "greater_equal" || n == "greater_or_equal") {
                out = CompareOp::GREATER_OR_EQUAL;
                return true;
            }
            if (n == "always") {
                out = CompareOp::ALWAYS;
                return true;
            }
            return false;
        }

        bool StencilOpFromName(const std::string &name, StencilOp &out)
        {
            const std::string n = Lower(name);
            if (n == "keep") {
                out = StencilOp::KEEP;
                return true;
            }
            if (n == "zero") {
                out = StencilOp::ZERO;
                return true;
            }
            if (n == "replace") {
                out = StencilOp::REPLACE;
                return true;
            }
            if (n == "increment_clamp" || n == "increment_and_clamp") {
                out = StencilOp::INCREMENT_AND_CLAMP;
                return true;
            }
            if (n == "decrement_clamp" || n == "decrement_and_clamp") {
                out = StencilOp::DECREMENT_AND_CLAMP;
                return true;
            }
            if (n == "invert") {
                out = StencilOp::INVERT;
                return true;
            }
            if (n == "increment_wrap" || n == "increment_and_wrap") {
                out = StencilOp::INCREMENT_AND_WRAP;
                return true;
            }
            if (n == "decrement_wrap" || n == "decrement_and_wrap") {
                out = StencilOp::DECREMENT_AND_WRAP;
                return true;
            }
            return false;
        }

        bool BlendFactorFromName(const std::string &name, BlendFactor &out)
        {
            const std::string n = Lower(name);
            if (n == "zero") {
                out = BlendFactor::ZERO;
                return true;
            }
            if (n == "one") {
                out = BlendFactor::ONE;
                return true;
            }
            if (n == "src_color") {
                out = BlendFactor::SRC_COLOR;
                return true;
            }
            if (n == "one_minus_src_color") {
                out = BlendFactor::ONE_MINUS_SRC_COLOR;
                return true;
            }
            if (n == "dst_color") {
                out = BlendFactor::DST_COLOR;
                return true;
            }
            if (n == "one_minus_dst_color") {
                out = BlendFactor::ONE_MINUS_DST_COLOR;
                return true;
            }
            if (n == "src_alpha") {
                out = BlendFactor::SRC_ALPHA;
                return true;
            }
            if (n == "one_minus_src_alpha") {
                out = BlendFactor::ONE_MINUS_SRC_ALPHA;
                return true;
            }
            if (n == "dst_alpha") {
                out = BlendFactor::DST_ALPHA;
                return true;
            }
            if (n == "one_minus_dst_alpha") {
                out = BlendFactor::ONE_MINUS_DST_ALPHA;
                return true;
            }
            if (n == "src_alpha_saturate") {
                out = BlendFactor::SRC_ALPHA_SATURATE;
                return true;
            }
            return false;
        }

        bool BlendOpFromName(const std::string &name, BlendOp &out)
        {
            const std::string n = Lower(name);
            if (n == "add") {
                out = BlendOp::ADD;
                return true;
            }
            if (n == "subtract" || n == "sub") {
                out = BlendOp::SUBTRACT;
                return true;
            }
            return false;
        }

        bool CullModeFromName(const std::string &name, CullingModeFlags &out)
        {
            const std::string n = Lower(name);
            if (n == "none") {
                out = CullingModeFlags();
                return true;
            }
            if (n == "front") {
                out = CullingModeFlags(CullModeFlagBits::FRONT);
                return true;
            }
            if (n == "back") {
                out = CullingModeFlags(CullModeFlagBits::BACK);
                return true;
            }
            if (n == "both" || n == "front_back") {
                out = CullingModeFlags(CullModeFlagBits::FRONT) | CullModeFlagBits::BACK;
                return true;
            }
            return false;
        }

        bool FrontFaceFromName(const std::string &name, FrontFace &out)
        {
            const std::string n = Lower(name);
            if (n == "cw") {
                out = FrontFace::CW;
                return true;
            }
            if (n == "ccw") {
                out = FrontFace::CCW;
                return true;
            }
            return false;
        }

        bool PolygonModeFromName(const std::string &name, PolygonMode &out)
        {
            const std::string n = Lower(name);
            if (n == "fill") {
                out = PolygonMode::FILL;
                return true;
            }
            if (n == "line") {
                out = PolygonMode::LINE;
                return true;
            }
            if (n == "point") {
                out = PolygonMode::POINT;
                return true;
            }
            return false;
        }

        bool PropertyTypeFromName(const std::string &name, MaterialPropertyType &out)
        {
            const std::string n = Lower(name);
            if (n == "float") {
                out = MaterialPropertyType::FLOAT;
                return true;
            }
            if (n == "float2") {
                out = MaterialPropertyType::FLOAT2;
                return true;
            }
            if (n == "float3") {
                out = MaterialPropertyType::FLOAT3;
                return true;
            }
            if (n == "float4") {
                out = MaterialPropertyType::FLOAT4;
                return true;
            }
            if (n == "mat4") {
                out = MaterialPropertyType::MAT4;
                return true;
            }
            if (n == "int") {
                out = MaterialPropertyType::INT;
                return true;
            }
            if (n == "uint") {
                out = MaterialPropertyType::UINT;
                return true;
            }
            if (n == "bool") {
                out = MaterialPropertyType::BOOL;
                return true;
            }
            if (n == "texture2d" || n == "texture") {
                out = MaterialPropertyType::TEXTURE2D;
                return true;
            }
            if (n == "texture_cube" || n == "cubemap") {
                out = MaterialPropertyType::TEXTURE_CUBE;
                return true;
            }
            if (n == "sampler") {
                out = MaterialPropertyType::SAMPLER;
                return true;
            }
            return false;
        }

        void WriteFloats(std::vector<uint8_t> &out, const rapidjson::Value &array)
        {
            std::vector<float> values;
            values.reserve(array.Size());
            for (const auto &element : array.GetArray()) {
                values.push_back(element.GetFloat());
            }
            out.resize(values.size() * sizeof(float));
            if (!values.empty()) {
                std::memcpy(out.data(), values.data(), out.size());
            }
        }

        // Explicit { "type": ..., "value": ... } form.
        void ParseExplicitProperty(const rapidjson::Value &obj, ParsedProperty &out)
        {
            if (!obj.HasMember("type") || !obj["type"].IsString()) {
                return;
            }
            MaterialPropertyType type;
            if (!PropertyTypeFromName(obj["type"].GetString(), type)) {
                LOG_W(TAG, "unknown explicit property type '%s'", obj["type"].GetString());
                return;
            }
            out.type = type;

            const rapidjson::Value *value = obj.HasMember("value") ? &obj["value"] : nullptr;
            if (value == nullptr) {
                return;
            }

            switch (type) {
            case MaterialPropertyType::FLOAT: {
                WritePOD(out.value, value->GetFloat());
                break;
            }
            case MaterialPropertyType::INT: {
                int32_t v = value->GetInt();
                WritePOD(out.value, v);
                break;
            }
            case MaterialPropertyType::UINT: {
                uint32_t v = value->GetUint();
                WritePOD(out.value, v);
                break;
            }
            case MaterialPropertyType::BOOL: {
                const uint8_t v = value->GetBool() ? 1u : 0u;
                WritePOD(out.value, v);
                break;
            }
            case MaterialPropertyType::FLOAT2:
            case MaterialPropertyType::FLOAT3:
            case MaterialPropertyType::FLOAT4:
            case MaterialPropertyType::MAT4:
                if (value->IsArray()) {
                    WriteFloats(out.value, *value);
                }
                break;
            case MaterialPropertyType::TEXTURE2D:
            case MaterialPropertyType::TEXTURE_CUBE:
            case MaterialPropertyType::SAMPLER:
                if (value->IsString()) {
                    out.texture = Uuid::CreateFromString(value->GetString());
                }
                break;
            default: break;
            }
        }

        void ParseProperty(const rapidjson::Value &value, ParsedProperty &out)
        {
            if (value.IsString()) {
                out.type    = MaterialPropertyType::TEXTURE2D;
                out.texture = Uuid::CreateFromString(value.GetString());
                return;
            }
            if (value.IsBool()) {
                out.type  = MaterialPropertyType::BOOL;
                out.value = {value.GetBool() ? 1u : 0u};
                return;
            }
            if (value.IsArray()) {
                const auto size = value.Size();
                if (size == 2) {
                    out.type = MaterialPropertyType::FLOAT2;
                } else if (size == 3) {
                    out.type = MaterialPropertyType::FLOAT3;
                } else if (size == 4) {
                    out.type = MaterialPropertyType::FLOAT4;
                } else {
                    LOG_W(TAG, "unsupported array property size %u", static_cast<uint32_t>(size));
                    return;
                }
                WriteFloats(out.value, value);
                return;
            }
            if (value.IsObject()) {
                ParseExplicitProperty(value, out);
                return;
            }
            if (value.IsNumber()) {
                out.type = MaterialPropertyType::FLOAT;
                WritePOD(out.value, value.GetFloat());
            }
        }

        void ParseBlendState(const rapidjson::Value &value, BlendState &out)
        {
            if (value.HasMember("enable")) {
                out.blendEn = value["enable"].GetBool();
            }
            if (value.HasMember("writeMask") && value["writeMask"].IsUint()) {
                out.writeMask = static_cast<uint8_t>(value["writeMask"].GetUint());
            }
            BlendFactor factor;
            if (value.HasMember("srcColor") && value["srcColor"].IsString() && BlendFactorFromName(value["srcColor"].GetString(), factor)) {
                out.srcColor = factor;
            }
            if (value.HasMember("dstColor") && value["dstColor"].IsString() && BlendFactorFromName(value["dstColor"].GetString(), factor)) {
                out.dstColor = factor;
            }
            if (value.HasMember("srcAlpha") && value["srcAlpha"].IsString() && BlendFactorFromName(value["srcAlpha"].GetString(), factor)) {
                out.srcAlpha = factor;
            }
            if (value.HasMember("dstAlpha") && value["dstAlpha"].IsString() && BlendFactorFromName(value["dstAlpha"].GetString(), factor)) {
                out.dstAlpha = factor;
            }
            BlendOp op;
            if (value.HasMember("colorOp") && value["colorOp"].IsString() && BlendOpFromName(value["colorOp"].GetString(), op)) {
                out.colorBlendOp = op;
            }
            if (value.HasMember("alphaOp") && value["alphaOp"].IsString() && BlendOpFromName(value["alphaOp"].GetString(), op)) {
                out.alphaBlendOp = op;
            }
        }

    } // namespace

    void ParseMaterialProperties(const rapidjson::Value &properties, std::vector<ParsedProperty> &out)
    {
        if (!properties.IsObject()) {
            return;
        }
        for (auto iter = properties.MemberBegin(); iter != properties.MemberEnd(); ++iter) {
            ParsedProperty property;
            property.name = iter->name.GetString();
            ParseProperty(iter->value, property);
            out.push_back(std::move(property));
        }
    }

    void ParseMaterialTechniques(const rapidjson::Value &techniques, std::vector<MaterialTechniqueData> &out)
    {
        if (!techniques.IsArray()) {
            return;
        }
        for (const auto &element : techniques.GetArray()) {
            if (!element.IsObject()) {
                continue;
            }
            MaterialTechniqueData data;
            if (element.HasMember("tag") && element["tag"].IsString()) {
                data.tag = element["tag"].GetString();
            }
            if (element.HasMember("shader") && element["shader"].IsString()) {
                data.shader.relativePath = element["shader"].GetString();
            }
            data.shader.entry = element.HasMember("entry") && element["entry"].IsString() ? element["entry"].GetString() : "main";
            if (element.HasMember("renderState")) {
                ParseMaterialRenderState(element["renderState"], data.renderState);
            }
            out.push_back(std::move(data));
        }
    }

    void ParseMaterialRenderState(const rapidjson::Value &renderState, MaterialRenderState &out)
    {
        if (!renderState.IsObject()) {
            return;
        }

        if (renderState.HasMember("depthTest")) {
            out.depthStencil.depthTest = renderState["depthTest"].GetBool();
        }
        if (renderState.HasMember("depthWrite")) {
            out.depthStencil.depthWrite = renderState["depthWrite"].GetBool();
        }
        if (renderState.HasMember("stencilTest")) {
            out.depthStencil.stencilTest = renderState["stencilTest"].GetBool();
        }
        CompareOp compareOp;
        if (renderState.HasMember("depthCompare") && renderState["depthCompare"].IsString() &&
            CompareOpFromName(renderState["depthCompare"].GetString(), compareOp)) {
            out.depthStencil.compareOp = compareOp;
        }
        StencilOp stencilOp;
        if (renderState.HasMember("stencilFail") && renderState["stencilFail"].IsString() &&
            StencilOpFromName(renderState["stencilFail"].GetString(), stencilOp)) {
            out.depthStencil.front.failOp = out.depthStencil.back.failOp = stencilOp;
        }
        if (renderState.HasMember("stencilPass") && renderState["stencilPass"].IsString() &&
            StencilOpFromName(renderState["stencilPass"].GetString(), stencilOp)) {
            out.depthStencil.front.passOp = out.depthStencil.back.passOp = stencilOp;
        }

        CullingModeFlags cullMode;
        if (renderState.HasMember("cullMode") && renderState["cullMode"].IsString() &&
            CullModeFromName(renderState["cullMode"].GetString(), cullMode)) {
            out.rasterState.cullMode = cullMode;
        }
        FrontFace frontFace;
        if (renderState.HasMember("frontFace") && renderState["frontFace"].IsString() &&
            FrontFaceFromName(renderState["frontFace"].GetString(), frontFace)) {
            out.rasterState.frontFace = frontFace;
        }
        PolygonMode polygonMode;
        if (renderState.HasMember("polygonMode") && renderState["polygonMode"].IsString() &&
            PolygonModeFromName(renderState["polygonMode"].GetString(), polygonMode)) {
            out.rasterState.polygonMode = polygonMode;
        }
        if (renderState.HasMember("depthBias")) {
            out.rasterState.depthBiasEnable = true;
            const auto &bias                = renderState["depthBias"];
            if (bias.HasMember("constant")) {
                out.rasterState.depthBiasConstantFactor = bias["constant"].GetFloat();
            }
            if (bias.HasMember("slope")) {
                out.rasterState.depthBiasSlopeFactor = bias["slope"].GetFloat();
            }
            if (bias.HasMember("clamp")) {
                out.rasterState.depthBiasClamp = bias["clamp"].GetFloat();
            }
        }
        if (renderState.HasMember("alphaToCoverage")) {
            out.alphaToCoverage = renderState["alphaToCoverage"].GetBool();
        }

        if (renderState.HasMember("blend")) {
            const auto &blend = renderState["blend"];
            if (blend.IsArray()) {
                for (const auto &element : blend.GetArray()) {
                    BlendState state;
                    ParseBlendState(element, state);
                    out.blendStates.push_back(state);
                }
            } else if (blend.IsObject()) {
                BlendState state;
                ParseBlendState(blend, state);
                out.blendStates.push_back(state);
            }
        }
    }

} // namespace sky::aurora::cook
