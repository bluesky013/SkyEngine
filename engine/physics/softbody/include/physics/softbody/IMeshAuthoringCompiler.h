#pragma once

#include <core/environment/Singleton.h>
#include <core/math/Matrix4.h>
#include <core/math/Transform.h>
#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <core/math/Vector4.h>
#include <core/util/Uuid.h>
#include <physics/PhysicsStatus.h>

#include <array>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace sky {
    class AssetBase;
}

namespace sky::phy {
    inline constexpr uint32_t AUTHORING_MAX_BONES = 256;

    struct AuthoringMeshPrimitive {
        // Source attributes are local; transform is baked into the compiled mesh model space.
        std::string           key;
        std::vector<Vector3>  positions;
        std::vector<Vector3>  normals;
        std::vector<Vector4>  tangents;
        std::vector<Vector2>  uvs;
        std::vector<Vector4>  colors;
        std::vector<uint32_t> indices;
        uint32_t              materialIndex = 0;
        Matrix4               transform     = Matrix4::Identity();
    };

    struct AuthoringMeshSource {
        std::string                         key;
        std::vector<std::string>            materialNames;
        std::vector<AuthoringMeshPrimitive> primitives;
    };

    struct AuthoringBone {
        std::string name;
        uint16_t    parent = UINT16_MAX;
        Transform   reference;
    };

    struct AuthoringSkinWeights {
        std::array<uint16_t, 4> bones{};
        std::array<float, 4>    weights{};
    };

    struct AuthoringPrimitiveBinding {
        std::string                       key;
        std::vector<AuthoringSkinWeights> vertices;
    };

    struct AuthoringMeshBinding {
        // Reference bones and control positions use the baked mesh model space.
        std::vector<AuthoringBone>             bones;
        std::vector<AuthoringPrimitiveBinding> primitives;
        uint64_t                               layoutSignature = 0;
        std::vector<Uuid>                      materialSlots;
    };

    struct AuthoringProductIds {
        Uuid sourceOwner;
        Uuid mesh;
        Uuid skin;
        Uuid skeleton;
    };

    struct AuthoringMeshProducts {
        std::vector<std::shared_ptr<AssetBase>> products;
        uint64_t                                skeletonLayout = 0;
        // Output vertex -> primitive/vertex in the supplied source, after any reordering.
        std::vector<std::array<uint32_t, 2>> vertexSources;
    };

    class IMeshAuthoringCompiler {
    public:
        virtual ~IMeshAuthoringCompiler() = default;
        virtual PhysicsStatus Load(std::span<const uint8_t> bytes, const std::string &extension, std::vector<AuthoringMeshSource> &out) const = 0;
        // Constructs owned products only; the recipe builder owns publication and child IDs.
        virtual PhysicsStatus Compile(const AuthoringMeshSource  &source,
                                      const AuthoringMeshBinding &binding,
                                      const AuthoringProductIds  &ids,
                                      AuthoringMeshProducts      &out) const = 0;
    };

    class MeshAuthoringCompilerRegistry : public Singleton<MeshAuthoringCompilerRegistry> {
        friend class Singleton<MeshAuthoringCompilerRegistry>;

    public:
        bool Register(std::shared_ptr<IMeshAuthoringCompiler> provider)
        {
            if (!provider) {
                return false;
            }
            const std::lock_guard lock(mutex);
            if (compiler) {
                return false;
            }
            compiler = std::move(provider);
            return true;
        }
        bool Unregister(const std::shared_ptr<IMeshAuthoringCompiler> &provider)
        {
            std::shared_ptr<IMeshAuthoringCompiler> retired;
            {
                const std::lock_guard lock(mutex);
                if (compiler.get() != provider.get() || compiler.owner_before(provider) || provider.owner_before(compiler)) {
                    return false;
                }
                retired = std::move(compiler);
            }
            return true;
        }
        std::shared_ptr<IMeshAuthoringCompiler> Acquire() const
        {
            const std::lock_guard lock(mutex);
            return compiler;
        }

    private:
        // The singleton owns the lock across DLLs; provider destruction occurs after unlocking.
        mutable std::mutex                      mutex;
        std::shared_ptr<IMeshAuthoringCompiler> compiler;
    };
} // namespace sky::phy
