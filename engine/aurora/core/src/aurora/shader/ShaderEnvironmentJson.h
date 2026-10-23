#pragma once

#include <aurora/rhi/ShaderArtifact.h>
#include <rapidjson/document.h>

namespace sky::aurora {
    inline bool ReadShaderEnvironmentJson(const rapidjson::Value &value, ShaderTargetEnvironment &environment)
    {
        if (!value.IsObject()) {
            return false;
        }
        for (const char *key : {"target", "platform", "codeFormat", "minimumVersion", "matrixLayout"}) {
            if (!value.HasMember(key) || !value[key].IsUint()) {
                return false;
            }
        }
        if (!value.HasMember("profile") || !value["profile"].IsString() || !value.HasMember("requiredCapabilities") ||
            !value["requiredCapabilities"].IsUint64() || !value.HasMember("options") || !value["options"].IsArray()) {
            return false;
        }
        ShaderTargetEnvironment parsed;
        parsed.target     = static_cast<ShaderTarget>(value["target"].GetUint());
        parsed.platform   = static_cast<ShaderPlatform>(value["platform"].GetUint());
        parsed.codeFormat = static_cast<ShaderCodeFormat>(value["codeFormat"].GetUint());
        parsed.profile.assign(value["profile"].GetString(), value["profile"].GetStringLength());
        parsed.minimumVersion       = value["minimumVersion"].GetUint();
        parsed.requiredCapabilities = value["requiredCapabilities"].GetUint64();
        parsed.matrixLayout         = static_cast<ShaderMatrixLayout>(value["matrixLayout"].GetUint());
        if (parsed.platform == ShaderPlatform::UNKNOWN || parsed.codeFormat == ShaderCodeFormat::UNKNOWN || parsed.profile.empty() ||
            parsed.profile.find('\0') != std::string::npos) {
            return false;
        }
        for (const auto &option : value["options"].GetArray()) {
            if (!option.IsString()) {
                return false;
            }
            std::string text(option.GetString(), option.GetStringLength());
            if (text.empty() || text.find('\0') != std::string::npos) {
                return false;
            }
            parsed.options.push_back(std::move(text));
        }
        auto        canonical = parsed;
        std::string error;
        if (!NormalizeShaderEnvironment(canonical, error) || canonical != parsed) {
            return false;
        }
        environment = std::move(parsed);
        return true;
    }
    inline const char *ShaderPlatformJsonName(ShaderPlatform platform)
    {
        switch (platform) {
        case ShaderPlatform::WINDOWS: return "windows";
        case ShaderPlatform::MACOS: return "macos";
        case ShaderPlatform::IOS: return "ios";
        case ShaderPlatform::ANDROID_OS: return "android";
        case ShaderPlatform::LINUX: return "linux";
        default: return "unknown";
        }
    }
} // namespace sky::aurora
