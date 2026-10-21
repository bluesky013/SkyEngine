// Emits variant schema headers; shared pass declarations need no generator.

#include <aurora/shader/gen/ShaderVariantGen.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static bool WriteOutput(const std::string &path, const std::string &contents)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cerr << "failed to open output: " << path << "\n";
        return false;
    }
    out << contents;
    out.close();
    if (!out) {
        std::cerr << "failed to write output: " << path << "\n";
        return false;
    }
    return true;
}

static int GenerateVariantHeader(const std::string &source, const std::string &outputPath, const std::string &schemaName)
{
    sky::aurora::ShaderVariantSchema schema;
    uint16_t                         reservedBits = 0;
    std::string                      error;
    if (!sky::aurora::ShaderVariantGen::Parse(source, schema, reservedBits, error)) {
        std::cerr << "variant parse failed: " << error << "\n";
        return 1;
    }

    std::string header;
    if (!sky::aurora::ShaderVariantGen::GenerateHeader(schemaName, schema, header)) {
        std::cerr << "variant codegen failed\n";
        return 1;
    }

    return WriteOutput(outputPath, header) ? 0 : 1;
}

int main(int argc, char *argv[])
{
    if (argc < 4 || argc > 5 || std::string(argv[1]) != "--variant") {
        std::cerr << "usage: ShaderHeaderTool --variant <input.slang> <output.variant.h> [schemaName]\n";
        return 1;
    }
    std::ifstream in(argv[2], std::ios::binary);
    if (!in) {
        std::cerr << "failed to open input: " << argv[2] << "\n";
        return 1;
    }
    std::ostringstream source;
    source << in.rdbuf();
    return GenerateVariantHeader(source.str(), argv[3], argc == 5 ? argv[4] : "Shader");
}
