//
// ShaderHeaderTool: offline codegen host tool.
// Reflects a .slang block header (no entry point) and emits the C++ mirror
// header (struct + static_assert + RgBlockDesc accessor) to the output path.
//

#include <aurora/shader/ShaderCompilerSlang.h>
#include <aurora/shader/gen/ShaderCodeGen.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

int main(int argc, char *argv[])
{
    if (argc < 3) {
        std::cerr << "usage: ShaderHeaderTool <input.slang> <output.h>\n";
        return 1;
    }

    const std::string inputPath  = argv[1];
    const std::string outputPath = argv[2];

    std::ifstream in(inputPath, std::ios::binary);
    if (!in) {
        std::cerr << "failed to open input: " << inputPath << "\n";
        return 1;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string source = ss.str();

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
