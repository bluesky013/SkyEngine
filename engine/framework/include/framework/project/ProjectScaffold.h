//
// Created on 2026/10/07.
//

#pragma once

#include <string>

namespace sky {

    // Generates the game source tree + build entry for a project from the engine
    // templates (`templates/game`), substituting the project name. Templates are
    // located via SKY_TEMPLATE_DIR (override) or <bundle>/templates.
    class ProjectScaffold {
    public:
        // Writes the scaffold for `name` into `projectDir` (the project root that
        // already contains the descriptor). Returns false on failure.
        static bool Scaffold(const std::string &projectDir, const std::string &name);

        // Module identity derived from the project name (sanitized identifier),
        // e.g. "TestProject" -> "TestProjectModule".
        static std::string ModuleName(const std::string &projectName);

        // Sanitized C/C++ identifier for the project name (used for the
        // scaffolded class/target names).
        static std::string Identifier(const std::string &projectName);

        // Directory holding the templates: SKY_TEMPLATE_DIR if set, else
        // <bundle>/templates.
        static std::string TemplateRoot();
    };

} // namespace sky
