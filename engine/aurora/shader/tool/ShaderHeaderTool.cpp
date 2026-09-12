//
// ShaderHeaderTool: offline codegen host tool.
//   ShaderHeaderTool <input.slang> <output.h>            -> C++ mirror header
//   ShaderHeaderTool --variant <input.slang> <out.variant.h> [schemaName]
//                                                         -> variant schema header
//

#include <aurora/shader/ShaderCompilerSlang.h>
#include <aurora/shader/gen/ShaderCodeGen.h>
#include <aurora/shader/gen/ShaderVariantGen.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>

static int GenerateMirrorHeader(const std::string &source, const std::string &outputPath)
{
    sky::aurora::ShaderCompilerSlang compiler;
    sky::aurora::ShaderReflection reflection;
    std::string error;
    if (!compiler.ReflectBlocks(source, sky::aurora::ShaderTarget::SPIRV, reflection, error)) {
        std::cerr << "reflect failed: " << error << "\n";
        return 1;
    }

    std::string header;
    for (const auto &block : reflection.blocks) {
        std::string one;
        if (!sky::aurora::ShaderCodeGen::GenerateCppHeader(block, one, error)) {
            std::cerr << "codegen failed: " << error << "\n";
            return 1;
        }
        header += one;
        header += "\n";
    }

    std::ofstream out(outputPath, std::ios::binary);
    if (!out) {
        std::cerr << "failed to open output: " << outputPath << "\n";
        return 1;
    }
    out << header;
    return 0;
}

static int GenerateVariantHeader(const std::string &source, const std::string &outputPath,
                                 const std::string &schemaName)
{
    sky::aurora::ShaderVariantSchema schema;
    uint16_t reservedBits = 0;
    std::string error;
    if (!sky::aurora::ShaderVariantGen::Parse(source, schema, reservedBits, error)) {
        std::cerr << "variant parse failed: " << error << "\n";
        return 1;
    }

    std::string header;
    if (!sky::aurora::ShaderVariantGen::GenerateHeader(schemaName, schema, header)) {
        std::cerr << "variant codegen failed\n";
        return 1;
    }

    std::ofstream out(outputPath, std::ios::binary);
    if (!out) {
        std::cerr << "failed to open output: " << outputPath << "\n";
        return 1;
    }
    out << header;
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        std::cerr << "usage:\n";
        std::cerr << "  ShaderHeaderTool <input.slang> <output.h>\n";
        std::cerr << "  ShaderHeaderTool --variant <input.slang> <output.variant.h> [schemaName]\n";
        return 1;
    }

    const bool variantMode = std::string(argv[1]) == "--variant";
    if (variantMode && argc < 4) {
        std::cerr << "--variant requires <input.slang> <output.variant.h>\n";
        return 1;
    }

    const std::string inputPath  = variantMode ? argv[2] : argv[1];
    const std::string outputPath = variantMode ? argv[3] : argv[2];

    std::ifstream in(inputPath, std::ios::binary);
    if (!in) {
        std::cerr << "failed to open input: " << inputPath << "\n";
        return 1;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string source = ss.str();

    if (variantMode) {
        std::string schemaName = argc >= 5 ? argv[4] : "Shader";
        return GenerateVariantHeader(source, outputPath, schemaName);
    }
    return GenerateMirrorHeader(source, outputPath);
}
