//
// ShaderRef helpers.
//

#include <aurora/shader/ShaderRef.h>

namespace sky::aurora {

    std::string NormalizeShaderPath(std::string_view path)
    {
        std::string out;
        out.reserve(path.size());

        // Strip leading "./" segments.
        std::string_view rest = path;
        while (rest.size() >= 2 && rest[0] == '.' && rest[1] == '/') {
            rest.remove_prefix(2);
        }

        bool lastWasSlash = false;
        for (char c : rest) {
            const char ch = (c == '\\') ? '/' : c;
            if (ch == '/') {
                if (out.empty() || lastWasSlash) {
                    continue; // drop leading / duplicate separators
                }
                lastWasSlash = true;
            } else {
                lastWasSlash = false;
            }
            out.push_back(ch);
        }
        return out;
    }

} // namespace sky::aurora
