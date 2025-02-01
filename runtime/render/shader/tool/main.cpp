//
// Created by blues on 2024/12/25.
//
#include <iostream>
#include <cxxopts.hpp>
#include <core/file/FileIO.h>
#include <core/logger/Logger.h>
#include <framework/application/Application.h>
#include <framework/platform/PlatformBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/interface/Interface.h>
#include <shader/ShaderCompiler.h>
#include <shader/ShaderFileSystem.h>
#include <shader/ShaderCacheManager.h>

using namespace sky;

class ShaderCompilerApp : public Application {
public:
    ShaderCompilerApp() = default;
    ~ShaderCompilerApp() override = default;

    bool Init(int argc, char** argv) override
    {
        cxxopts::Options options("ShaderCompiler Application", "SkyEngine ShaderTool");
        options.allow_unrecognised_options();

        options.add_options()("s,search", "Shader Search Path", cxxopts::value<std::vector<std::string>>());
        options.add_options()("f,file", "Single Shader", cxxopts::value<std::string>());
        options.add_options()("o,opt", "Shader Opt Level", cxxopts::value<uint32_t>());
        options.add_options()("t,target", "Compile target(SpirV, Metal, DX, GLSL)", cxxopts::value<std::vector<std::string>>());
        options.add_options()("i,intermediate", "Compile Intermediate Directory", cxxopts::value<std::string>());
        options.add_options()("p,prefix", "Shader Path Prefix", cxxopts::value<std::string>());
        options.add_options()("v,spv", "Save SpirV");
        options.add_options()("h,help", "Print usage");

        auto result = options.parse(argc, argv);

        if (result.count("help") != 0u) {
            printf("%s", options.help().c_str());
            return false;
        }

        if (result.count("search") != 0u) {
            auto searchPaths = result["search"].as<std::vector<std::string>>();
            for (auto &path : searchPaths) {
                ShaderFileSystem::Get()->AddSearchPath(path);
            }
        }

        if (result.count("intermediate") != 0u) {
            FilePath intermediate = result["intermediate"].as<std::string>();
            ShaderFileSystem::Get()->SetIntermediateFS(new NativeFileSystem(intermediate / FilePath("shaders")));
        }
        ShaderFileSystem::Get()->SetWorkFS(new NativeFileSystem("Shaders"));

        if (result.count("file") != 0u) {
            singleShader = result["file"].as<std::string>();
        }

        if (result.count("spv") != 0u) {
            saveSpirV = true;
        }

        if (result.count("target") != 0u) {
            auto array = result["target"].as<std::vector<std::string>>();
            for (auto &t : array) {
                if (t == "SpirV") {
                    targets.emplace_back(ShaderCompileTarget::SPIRV);
                } else if (t == "Metal") {
                    targets.emplace_back(ShaderCompileTarget::MSL);
                } else if (t == "DX") {
                }
            }
        }

        if (result.count("opt") != 0u) {
        }
        return Application::Init(argc, argv);
    }

    void LoadConfigs() override
    {
        std::unordered_map<std::string, ModuleInfo> modules = {};
        modules.emplace("ShaderCompiler", ModuleInfo{"ShaderCompiler", {}});

        for (auto &[key, info] : modules) {
            moduleManager->RegisterModule(info);
        }
    }

    static void ReplaceShaderName(std::string &name)
    {
        std::replace(name.begin(), name.end(), '\\', '_');
        std::replace(name.begin(), name.end(), '/', '_');
    }

    static void ReplaceShaderBinaryName(std::string &name, const std::string& entry, const ShaderVariantKey &key)
    {
        ReplaceShaderName(name);
        auto iter = name.find_last_of('.');

        std::string entryWithOpt = std::string("_") + entry + "_" + key.ToString();
        name.insert(iter, entryWithOpt);
    }

    static bool ContainsEntry(const std::string &source, const std::string &entry)
    {
        auto iter = source.find(entry);
        return iter != std::string::npos;
    }

    void SaveBinaryIntermediate(ShaderCompilerBase* compiler, const std::string &name, const std::string& entry,
        const ShaderVariantKey &key, ShaderCompileTarget target, const ShaderBuildResult& result) const
    {
        std::string replacedName = name;
        ReplaceShaderBinaryName(replacedName, entry, key);

        if (target == ShaderCompileTarget::SPIRV && saveSpirV) {
            std::string source = compiler->Disassemble(result.data, target);

            auto path = FilePath(replacedName);
            path.ReplaceExtension(".spv");

            auto fs = ShaderFileSystem::Get()->GetIntermediateBinaryFS(target);
            auto file = fs->CreateOrOpenFile(path);
            auto archive = file->WriteAsArchive();
            archive->SaveRaw(source.c_str(), source.length());
        }
    }

    static void LoadDefaultEntries(const std::string &source, std::vector<std::pair<rhi::ShaderStageFlagBit, Name>> &entries)
    {
        if (ContainsEntry(source, "VSMain")) {
            entries.emplace_back(std::pair<rhi::ShaderStageFlagBit, Name>{rhi::ShaderStageFlagBit::VS, Name("VSMain")});
        }

        if (ContainsEntry(source, "FSMain")) {
            entries.emplace_back(std::pair<rhi::ShaderStageFlagBit, Name>{rhi::ShaderStageFlagBit::FS, Name("FSMain")});
        }

        if (ContainsEntry(source, "CSMain")) {
            entries.emplace_back(std::pair<rhi::ShaderStageFlagBit, Name>{rhi::ShaderStageFlagBit::CS, Name("CSMain")});
        }
    }

    static void ProcessShaderOptionItems(ShaderVariantList& list, std::string& source)
    {
        std::vector<ShaderOptionItem> items = ShaderCompiler::PreProcess(source);
        for (const auto &item : items) {
            list.AddOptionItem(item);
        }
    }

    static void LoadShaderVariants(const FilePath &path, ShaderVariantList& list,
        std::vector<std::pair<rhi::ShaderStageFlagBit, Name>> &entries)
    {
        std::string json;
        ReadString(path, json);
        if (!json.empty()) {
            rapidjson::Document document;
            document.Parse(json.c_str());

            if (document.HasMember("entries")) {
                auto array = document["entries"].GetArray();
                for (auto &ele : array) {
                    rhi::ShaderStageFlagBit stage = ShaderCompiler::GetShaderStage(ele["stage"].GetString());
                    Name name(ele["name"].GetString());
                    entries.emplace_back(std::pair<rhi::ShaderStageFlagBit, Name>{stage, name});
                }
            }
        }
    }

    static std::string LoadAndSaveShader(const std::string &name)
    {
        std::string source = ShaderCompiler::Get()->LoadShader(name);
        ShaderFileSystem::Get()->SaveCacheSource(Name(name.c_str()), source);

        return source;
    }

    void CompileBinary(const std::string &name, const MD5& md5, const ShaderSourceDesc &source, const ShaderVariantList &list)
    {
        if (getCompilerFn == nullptr) {
            return;
        }

        auto permutations = list.GeneratePermutation();

        Name shaderName(name.c_str());
        Name shaderEntry(source.entry.c_str());

        for (auto &target : targets) {
            ShaderCompileOption option = {};
            option.target = target;
            option.option = new ShaderOption();

            for (auto &key : permutations) {
                list.FillShaderOption(*option.option, key);

                ShaderBuildResult result = {};
                auto *targetCompiler = getCompilerFn(target);
                if (targetCompiler->CompileBinary(source, option, result)) {
                    ShaderCacheManager::Get()->SaveBinaryCache(shaderName, target, shaderEntry, key, result);
                    SaveBinaryIntermediate(targetCompiler, name, source.entry, key, target, result);
                }
            }
        }
    }

    void CompileShader(const std::string &name)
    {
        auto path = ShaderCompiler::Get()->GetShaderPath(name);
        path /= name;
        LOG_I("ShaderTool", "compiling shader: %s\n", path.GetStr().c_str());

        ShaderSourceDesc desc = {};
        desc.source = LoadAndSaveShader(name);

        // load variants
        path.ReplaceExtension(".variants");
        std::vector<std::pair<rhi::ShaderStageFlagBit, Name>> entries;
        ShaderVariantList list;
        ProcessShaderOptionItems(list, desc.source);
        LoadShaderVariants(path, list, entries);

        if (entries.empty()) {
            LoadDefaultEntries(desc.source, entries);
        }

        auto md5 = ShaderCompiler::CalculateShaderMD5(desc.source);

        for (const auto &[stage, entry] : entries) {
            desc.stage = stage;
            desc.entry = entry.GetStr().data();

            CompileBinary(name, md5, desc, list);
        }
    }

    void Run()
    {
        auto *mm = Interface<ISystemNotify>::Get()->GetApi()->GetModuleManager();
        getCompilerFn = mm->GetFunctionFomModule<GetBinaryCompilerFunc>("ShaderCompiler", "GetBinaryCompiler");

        ShaderCompiler::Get()->LoadPipelineOptions(passOptions);

        for (auto &target : targets) {
            ShaderCacheManager::Get()->LoadMappingFile(target);
        }

        if (!singleShader.empty()) {
            CompileShader(singleShader);
        }

        ShaderCacheManager::Get()->SaveMappingFile();
    }

private:
    GetBinaryCompilerFunc getCompilerFn = nullptr;

    bool saveSpirV = false;

    std::string singleShader;
    std::string passOptions = "shaders/pipeline/pass_options.hlslh";
    std::vector<ShaderCompileTarget> targets;
};

int main(int argc, char *argv[])
{
    Platform* platform = Platform::Get();
    if (!platform->Init({})) {
        return -1;
    }

    ShaderCompilerApp app;
    if (app.Init(argc, argv)) {
        app.Run();
    }
    return 0;
}