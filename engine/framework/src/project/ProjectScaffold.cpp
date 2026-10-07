//
// Created on 2026/10/07.
//

#include <framework/project/ProjectScaffold.h>

#include <core/logger/Logger.h>
#include <framework/platform/PlatformBase.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

static const char *TAG = "ProjectScaffold";

namespace sky {

    namespace {
        constexpr const char *kToken      = "{{PROJECT_NAME}}";
        constexpr const char *kTemplateSub = "game";

        std::string ReplaceAll(std::string value, const std::string &from, const std::string &to)
        {
            size_t pos = 0;
            while ((pos = value.find(from, pos)) != std::string::npos) {
                value.replace(pos, from.size(), to);
                pos += to.size();
            }
            return value;
        }

        std::string ReadFile(const std::filesystem::path &path)
        {
            std::ifstream in(path, std::ios::binary);
            std::ostringstream ss;
            ss << in.rdbuf();
            return ss.str();
        }

        bool WriteFile(const std::filesystem::path &path, const std::string &data)
        {
            std::error_code error;
            std::filesystem::create_directories(path.parent_path(), error);
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            out << data;
            return static_cast<bool>(out);
        }
    } // namespace

    std::string ProjectScaffold::ModuleName(const std::string &projectName)
    {
        return Identifier(projectName) + "Module";
    }

    std::string ProjectScaffold::Identifier(const std::string &projectName)
    {
        std::string id;
        id.reserve(projectName.size());
        for (const char ch : projectName) {
            if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_') {
                id += ch;
            }
        }
        if (id.empty() || (id[0] >= '0' && id[0] <= '9')) {
            id = "Game" + id;
        }
        return id;
    }

    std::string ProjectScaffold::TemplateRoot()
    {
        if (const char *override = std::getenv("SKY_TEMPLATE_DIR"); override != nullptr && override[0] != '\0') {
            return override;
        }
        std::string base = Platform::Get() != nullptr ? Platform::Get()->GetBundlePath() : std::string{};
        if (base.empty()) {
            base = ".";
        }
        return (std::filesystem::path(base) / "templates").string();
    }

    bool ProjectScaffold::Scaffold(const std::string &projectDir, const std::string &name)
    {
        const std::string identifier = Identifier(name);
        const std::filesystem::path templateDir = std::filesystem::path(TemplateRoot()) / kTemplateSub;
        const std::filesystem::path targetDir   = projectDir;

        std::error_code error;
        if (!std::filesystem::is_directory(templateDir, error)) {
            LOG_E(TAG, "template dir not found: %s", templateDir.string().c_str());
            return false;
        }

        for (const auto &entry : std::filesystem::recursive_directory_iterator(templateDir, error)) {
            if (error) {
                LOG_E(TAG, "iterate failed: %s", error.message().c_str());
                return false;
            }
            if (!entry.is_regular_file()) {
                continue;
            }
            const auto rel = std::filesystem::relative(entry.path(), templateDir, error);
            // {{PROJECT_NAME}} is the sanitized identifier; templates append the
            // Module suffix themselves (e.g. {{PROJECT_NAME}}Module).
            const auto outPath = targetDir / ReplaceAll(rel.string(), kToken, identifier);
            const std::string content = ReadFile(entry.path());
            if (!WriteFile(outPath, ReplaceAll(content, kToken, identifier))) {
                LOG_E(TAG, "write failed: %s", outPath.string().c_str());
                return false;
            }
        }

        LOG_I(TAG, "scaffolded project '%s' (module %s)", name.c_str(), ModuleName(name).c_str());
        return true;
    }

} // namespace sky
