//
// ShaderVariantGen implementation: parse the @variant comment block and emit a
// C++ schema header.
//

#include <aurora/shader/gen/ShaderVariantGen.h>

#include <sstream>
#include <cstdlib>

namespace sky::aurora {

    namespace {

        std::string Trim(const std::string &in)
        {
            const size_t first = in.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) {
                return {};
            }
            const size_t last = in.find_last_not_of(" \t\r\n");
            return in.substr(first, last - first + 1);
        }

        // strip a leading "//" comment marker
        std::string StripComment(const std::string &in)
        {
            const size_t pos = in.find("//");
            return Trim(pos != std::string::npos ? in.substr(pos + 2) : in);
        }

        // parse "NAME : spec(ID, WIDTH) = DEF" or "NAME : bool = DEF"
        struct ParsedEntry {
            std::string name;
            bool        isSpec   = false;
            uint32_t    specId   = 0;
            uint8_t     bitWidth = 1;
            uint32_t    defaultValue = 0;
        };

        bool ParseEntry(const std::string &line, ParsedEntry &out)
        {
            const size_t colon = line.find(':');
            if (colon == std::string::npos) {
                return false;
            }
            const std::string name = Trim(line.substr(0, colon));
            const std::string rest = Trim(line.substr(colon + 1));

            const size_t eq = rest.find('=');
            const std::string kind    = Trim(eq != std::string::npos ? rest.substr(0, eq) : rest);
            const std::string defaultStr = Trim(eq != std::string::npos ? rest.substr(eq + 1) : "0");

            if (name.empty()) {
                return false;
            }
            out.name         = name;
            out.defaultValue = static_cast<uint32_t>(std::strtoul(defaultStr.c_str(), nullptr, 10));

            if (kind == "bool") {
                out.isSpec   = false;
                out.bitWidth = 1;
                return true;
            }
            if (kind.rfind("spec(", 0) == 0) {
                // spec(ID, WIDTH)
                const size_t open  = kind.find('(');
                const size_t comma = kind.find(',');
                const size_t close = kind.find(')');
                if (open == std::string::npos || comma == std::string::npos ||
                    close == std::string::npos) {
                    return false;
                }
                const std::string idStr = Trim(kind.substr(open + 1, comma - open - 1));
                const std::string wStr  = Trim(kind.substr(comma + 1, close - comma - 1));
                out.isSpec   = true;
                out.specId   = static_cast<uint32_t>(std::strtoul(idStr.c_str(), nullptr, 10));
                out.bitWidth = static_cast<uint8_t>(std::strtoul(wStr.c_str(), nullptr, 10));
                return out.bitWidth != 0;
            }
            return false;
        }
    } // namespace

    bool ShaderVariantGen::Parse(const std::string &source, ShaderVariantSchema &out,
                                 uint16_t &reservedBits, std::string &error)
    {
        const size_t marker = source.find("@variant");
        if (marker == std::string::npos) {
            error = "no @variant block found";
            return false;
        }

        out.sources.clear();
        out.entries.clear();
        out.totalBits = 0;
        reservedBits  = 0;

        std::istringstream ss(source.substr(marker));
        std::string line;
        int currentSource = -1;
        uint16_t runningOffset = 0;

        while (std::getline(ss, line)) {
            const std::string content = StripComment(line);
            if (content.empty()) {
                continue;
            }

            // end marker
            if (content.find("====") != std::string::npos && content.find("@source") == std::string::npos) {
                if (content.find("@variant") == std::string::npos) {
                    break;
                }
                continue;
            }

            if (content.rfind("@reserved", 0) == 0) {
                const std::string rest = Trim(content.substr(9));
                reservedBits = static_cast<uint16_t>(std::strtoul(rest.c_str(), nullptr, 10));
                continue;
            }

            if (content.rfind("@source", 0) == 0) {
                const std::string name = Trim(content.substr(7));
                if (name.empty()) {
                    error = "empty @source name";
                    return false;
                }
                for (const auto &s : out.sources) {
                    if (s.name == Name(name.c_str())) {
                        error = "duplicate source: " + name;
                        return false;
                    }
                }
                out.sources.push_back({Name(name.c_str()), runningOffset, 0});
                currentSource = static_cast<int>(out.sources.size()) - 1;
                continue;
            }

            // entry line
            ParsedEntry entry;
            if (!ParseEntry(content, entry)) {
                if (content.rfind("@", 0) == 0) {
                    continue; // ignore unknown directives
                }
                error = "malformed entry: " + content;
                return false;
            }
            if (currentSource < 0) {
                error = "entry before @source: " + entry.name;
                return false;
            }
            for (const auto &e : out.entries) {
                if (e.key == Name(entry.name.c_str())) {
                    error = "duplicate key: " + entry.name;
                    return false;
                }
            }

            auto &src = out.sources[static_cast<size_t>(currentSource)];
            const uint16_t relOffset = static_cast<uint16_t>(runningOffset - src.bitOffset);
            out.entries.push_back({Name(entry.name.c_str()), src.name, relOffset,
                                   entry.bitWidth, entry.defaultValue, entry.isSpec, entry.specId});
            runningOffset = static_cast<uint16_t>(runningOffset + entry.bitWidth);
            src.bitWidth  = static_cast<uint8_t>(runningOffset - src.bitOffset);
        }

        out.totalBits = runningOffset;
        if (out.totalBits > ShaderVariantKey::kMaxBits) {
            error = "variant schema exceeds 128 bits";
            return false;
        }
        return true;
    }

    bool ShaderVariantGen::ParseVertex(const std::string &source,
                                       std::vector<VertexVariantDef> &out,
                                       std::string &error)
    {
        const size_t marker = source.find("@vertex");
        if (marker == std::string::npos) {
            error = "no @vertex block found";
            return false;
        }

        out.clear();
        std::istringstream ss(source.substr(marker));
        std::string line;
        while (std::getline(ss, line)) {
            const std::string content = StripComment(line);
            if (content.empty()) {
                continue;
            }
            if (content.find("====") != std::string::npos) {
                if (content.find("@vertex") == std::string::npos) {
                    break; // end marker
                }
                continue; // start marker line
            }

            // "SWITCH : SEMANTIC1 SEMANTIC2 ..."
            const size_t colon = content.find(':');
            if (colon == std::string::npos) {
                continue;
            }
            const std::string name    = Trim(content.substr(0, colon));
            const std::string semList = Trim(content.substr(colon + 1));
            if (name.empty() || semList.empty()) {
                error = "malformed @vertex entry: " + content;
                return false;
            }

            VertexVariantDef def;
            def.name = Name(name.c_str());
            std::istringstream semStream(semList);
            std::string semName;
            while (semStream >> semName) {
                VertexSemantic semantic;
                if (!ParseVertexSemantic(semName, semantic)) {
                    error = "unknown vertex semantic: " + semName;
                    return false;
                }
                def.semantics.push_back(semantic);
            }
            out.push_back(std::move(def));
        }
        return true;
    }

    bool ShaderVariantGen::GenerateHeader(const std::string &schemaName,
                                          const ShaderVariantSchema &schema, std::string &out)
    {
        std::ostringstream ss;
        ss << "// generated: do not edit\n";
        ss << "#pragma once\n\n";
        ss << "#include <aurora/shader/ShaderVariant.h>\n\n";
        ss << "namespace sky::aurora::generated {\n\n";
        ss << "    inline const ShaderVariantSchema &Get" << schemaName << "VariantSchema()\n";
        ss << "    {\n";
        ss << "        static const ShaderVariantSchema s = [] {\n";
        ss << "            ShaderVariantSchema v;\n";
        for (const auto &source : schema.sources) {
            ss << "            v.sources.push_back({Name(\"" << source.name.GetStr()
               << "\"), " << source.bitOffset << ", " << static_cast<int>(source.bitWidth) << "});\n";
        }
        for (const auto &entry : schema.entries) {
            ss << "            v.entries.push_back({Name(\"" << entry.key.GetStr()
               << "\"), Name(\"" << entry.source.GetStr() << "\"), " << entry.bitOffset << ", "
               << static_cast<int>(entry.bitWidth) << ", " << entry.defaultValue << ", "
               << (entry.isSpec ? "true" : "false") << ", " << entry.specId << "});\n";
        }
        ss << "            v.totalBits = " << schema.totalBits << ";\n";
        ss << "            return v;\n";
        ss << "        }();\n";
        ss << "        return s;\n";
        ss << "    }\n\n";
        ss << "} // namespace sky::aurora::generated\n";
        out = ss.str();
        return true;
    }

} // namespace sky::aurora
