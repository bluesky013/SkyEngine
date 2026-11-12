#pragma once

#include <editor/core/asset/AssetGeneratorRegistry.h>
#include <physics/softbody/IMeshAuthoringCompiler.h>

#include <array>
#include <span>
#include <unordered_map>

namespace sky::aurora::editor {
    inline constexpr Uuid MESH_GENERATOR_INPUT_INTERFACE  = Uuid::CreateFromString("57528b78-cfa9-4da6-82c3-f27b7edf1cc2");
    inline constexpr Uuid MESH_GENERATOR_OUTPUT_INTERFACE = Uuid::CreateFromString("bfef28dc-449d-42aa-b0fc-fdafed102353");

    enum class MeshControlMode : uint8_t { Free, WorldFixed, RootDriven, END };

    struct MeshControlPoint {
        std::string     id;
        Vector3         position;
        MeshControlMode mode                                       = MeshControlMode::Free;
        bool            operator==(const MeshControlPoint &) const = default;
    };

    struct MeshGeneratorInputData {
        Uuid                          sourceOwner;
        std::string                   sourceKey;
        phy::AuthoringMeshSource      mesh;
        std::vector<MeshControlPoint> controls;
    };

    class IMeshGeneratorInput {
    public:
        virtual ~IMeshGeneratorInput()                             = default;
        virtual const MeshGeneratorInputData &GetMeshInput() const = 0;
    };

    struct MeshGeneratorOutputData {
        Uuid                                 sourceOwner;
        std::string                          sourceKey;
        phy::AuthoringMeshBinding            binding;
        std::vector<MeshControlPoint>        controls;
        std::vector<Vector3>                 cagePoints;
        std::vector<std::array<uint32_t, 4>> tetrahedra;
        std::string                          recipeText;
        // Builder-recognized extension without a leading dot; never an imported mesh extension.
        std::string                                      recipeExtension;
        std::vector<std::pair<std::string, std::string>> diagnostics;
    };

    class IMeshGeneratorOutput {
    public:
        virtual ~IMeshGeneratorOutput()                              = default;
        virtual const MeshGeneratorOutputData &GetMeshOutput() const = 0;
    };

    struct MeshGeneratorRecipeState {
        Uuid                          sourceOwner;
        std::string                   sourceKey;
        std::string                   generatorId;
        std::vector<MeshControlPoint> controls;
        Any                           settings;
    };

    class IMeshGeneratorRecipeCodec {
    public:
        virtual ~IMeshGeneratorRecipeCodec()                                                                                  = default;
        virtual sky::editor::AssetGeneratorStatus Decode(std::span<const uint8_t> bytes, MeshGeneratorRecipeState &out) const = 0;
    };

    // Main-thread registration; an acquired codec owns its provider through recipe decoding.
    class MeshGeneratorRecipeRegistry : public Singleton<MeshGeneratorRecipeRegistry> {
        friend class Singleton<MeshGeneratorRecipeRegistry>;

    public:
        bool Register(const std::string &extension, const Uuid &owner, std::shared_ptr<IMeshGeneratorRecipeCodec> codec)
        {
            return !extension.empty() && owner && codec && codecs.emplace(extension, Entry{owner, std::move(codec)}).second;
        }
        void UnregisterOwner(const Uuid &owner)
        {
            std::erase_if(codecs, [&](const auto &entry) { return entry.second.owner == owner; });
        }
        std::shared_ptr<IMeshGeneratorRecipeCodec> Acquire(const std::string &extension) const
        {
            const auto found = codecs.find(extension);
            return found == codecs.end() ? nullptr : found->second.codec;
        }

    private:
        struct Entry {
            Uuid                                       owner;
            std::shared_ptr<IMeshGeneratorRecipeCodec> codec;
        };
        std::unordered_map<std::string, Entry> codecs;
    };
} // namespace sky::aurora::editor
