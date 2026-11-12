#include <softbody/assets/SoftBodyAsset.h>

#include <framework/world/ComponentFactory.h>
#include <softbody/assets/SoftBodyComponentData.h>

#include <framework/asset/AssetManager.h>

#include <algorithm>
#include <bit>
#include <set>

namespace sky::softbody {
    namespace {
        constexpr uint32_t MAGIC                = 0x31424653;
        constexpr uint32_t MAX_CONSTRAINTS      = 262144;
        constexpr uint32_t MAX_CONTROL_ID_BYTES = 256;
        phy::PhysicsStatus Invalid(const char *message)
        {
            return {phy::PhysicsError::InvalidArgument, message};
        }
        struct Writer {
            std::vector<uint8_t> bytes;
            void                 U32(uint32_t value)
            {
                for (unsigned i = 0; i < 4; ++i)
                    bytes.push_back(static_cast<uint8_t>(value >> (8 * i)));
            }
            void U64(uint64_t value)
            {
                U32(static_cast<uint32_t>(value));
                U32(static_cast<uint32_t>(value >> 32));
            }
            void Float(float value)
            {
                U32(std::bit_cast<uint32_t>(value));
            }
            void Vec(const Vector3 &value)
            {
                Float(value.x);
                Float(value.y);
                Float(value.z);
            }
            void Id(const Uuid &id)
            {
                bytes.insert(bytes.end(), std::begin(id.data), std::end(id.data));
            }
            void Text(const std::string &value)
            {
                U32(static_cast<uint32_t>(value.size()));
                bytes.insert(bytes.end(), value.begin(), value.end());
            }
        };
        struct Reader {
            std::span<const uint8_t> bytes;
            size_t                   cursor = 0;
            bool                     U32(uint32_t &value)
            {
                if (bytes.size() - cursor < 4)
                    return false;
                value = 0;
                for (unsigned i = 0; i < 4; ++i)
                    value |= uint32_t(bytes[cursor++]) << (8 * i);
                return true;
            }
            bool U64(uint64_t &value)
            {
                uint32_t a = 0, b = 0;
                if (!U32(a) || !U32(b))
                    return false;
                value = uint64_t(a) | (uint64_t(b) << 32);
                return true;
            }
            bool Float(float &value)
            {
                uint32_t bits = 0;
                if (!U32(bits))
                    return false;
                value = std::bit_cast<float>(bits);
                return true;
            }
            bool Vec(Vector3 &value)
            {
                return Float(value.x) && Float(value.y) && Float(value.z);
            }
            bool Id(Uuid &id)
            {
                if (bytes.size() - cursor < 16)
                    return false;
                std::copy_n(bytes.data() + cursor, 16, id.data);
                cursor += 16;
                return true;
            }
            bool Count(uint32_t &count, uint32_t limit)
            {
                return U32(count) && count <= limit;
            }
            bool Text(std::string &value)
            {
                uint32_t size = 0;
                if (!Count(size, MAX_CONTROL_ID_BYTES) || size > bytes.size() - cursor)
                    return false;
                value.assign(reinterpret_cast<const char *>(bytes.data() + cursor), size);
                cursor += size;
                return true;
            }
        };
        class SoftBodyHandler final : public AssetHandlerBase {
        public:
            AssetPtr CreateAsset() override
            {
                return std::make_shared<Asset<SoftBodyAsset>>();
            }
            bool Load(IStreamArchive &archive, const AssetPtr &asset) override
            {
                std::vector<uint8_t> bytes;
                auto                &stream = archive.GetStream();
                char                 buffer[4096];
                while (stream.read(buffer, sizeof(buffer)) || stream.gcount()) {
                    const auto count = static_cast<size_t>(stream.gcount());
                    if (count > MAX_SOFT_BODY_PRODUCT_BYTES - bytes.size())
                        return false;
                    bytes.insert(bytes.end(), buffer, buffer + count);
                }
                if (stream.bad() || !stream.eof())
                    return false;
                stream.clear();
                return static_cast<bool>(DecodeSoftBodyAsset(bytes, std::static_pointer_cast<Asset<SoftBodyAsset>>(asset)->Data()));
            }
            bool SaveChecked(OStreamArchive &archive, const AssetPtr &asset) override
            {
                std::vector<uint8_t> bytes;
                if (!EncodeSoftBodyAsset(std::static_pointer_cast<Asset<SoftBodyAsset>>(asset)->Data(), bytes))
                    return false;
                archive.GetStream().write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                return archive.GetStream().good();
            }
        };
    } // namespace

    phy::PhysicsStatus ValidateSoftBodyAsset(const SoftBodyAssetData &data)
    {
        if ((data.version != 1 && data.version != 2) ||
            static_cast<uint32_t>(data.normalPolicy) >= static_cast<uint32_t>(SoftBodyNormalPolicy::END) ||
            (data.version == 1 && data.normalPolicy != SoftBodyNormalPolicy::TetGradient) || !data.mesh || !data.skin || !data.skeleton ||
            data.mesh == data.skin || data.mesh == data.skeleton || data.skin == data.skeleton || !data.skeletonLayout)
            return Invalid("Invalid soft body product identities or schema");
        const auto &body = data.body;
        if (body.transform.translation != VEC3_ZERO || body.transform.scale != VEC3_ONE || body.transform.rotation.x != 0 ||
            body.transform.rotation.y != 0 || body.transform.rotation.z != 0 || body.transform.rotation.w != 1)
            return Invalid("Soft body product root must be identity");
        const auto &mesh = body.mesh;
        if (mesh.vertices.size() > MAX_SOFT_BODY_PARTICLES || data.controlIds.size() != mesh.vertices.size() || mesh.tetrahedra.empty() ||
            mesh.edges.size() > MAX_CONSTRAINTS || mesh.faces.size() > MAX_CONSTRAINTS || mesh.tetrahedra.size() > MAX_CONSTRAINTS)
            return Invalid("Soft body product exceeds the bone or topology budget");
        if (const auto status = phy::ValidateSoftBodyDesc(body); !status)
            return status;
        std::set<std::string> ids;
        for (const auto &id : data.controlIds)
            if (id.empty() || id.size() > MAX_CONTROL_ID_BYTES || id.find('\0') != std::string::npos || !ids.insert(id).second)
                return Invalid("Soft body control identities must be nonempty and unique");
        std::vector<bool> supported(mesh.vertices.size());
        for (const auto &tet : mesh.tetrahedra)
            for (const auto index : tet.vertices)
                supported[index] = true;
        if (std::find(supported.begin(), supported.end(), false) != supported.end())
            return Invalid("Every control requires a tetrahedral normal neighbourhood");
        return {};
    }

    phy::PhysicsStatus EncodeSoftBodyAsset(const SoftBodyAssetData &data, std::vector<uint8_t> &out)
    {
        if (const auto status = ValidateSoftBodyAsset(data); !status)
            return status;
        Writer w;
        w.U32(MAGIC);
        w.U32(data.version);
        w.Id(data.mesh);
        w.Id(data.skin);
        w.Id(data.skeleton);
        w.U64(data.skeletonLayout);
        const auto &b = data.body;
        w.U32(b.filter.group);
        w.U32(b.filter.mask);
        w.U32(b.solverIterations);
        w.Float(b.pressure);
        w.Float(b.gravityFactor);
        w.Float(b.linearDamping);
        w.Float(b.maxLinearVelocity);
        w.Float(b.vertexRadius);
        w.Float(b.friction);
        w.Float(b.restitution);
        w.U32(b.allowSleeping ? 1 : 0);
        w.U32(static_cast<uint32_t>(b.mesh.vertices.size()));
        for (size_t i = 0; i < b.mesh.vertices.size(); ++i) {
            const auto &v = b.mesh.vertices[i];
            w.Vec(v.position);
            w.Vec(v.velocity);
            w.Float(v.inverseMass);
            w.U32(static_cast<uint32_t>(v.mode));
            w.Text(data.controlIds[i]);
        }
        w.U32(static_cast<uint32_t>(b.mesh.edges.size()));
        for (const auto &e : b.mesh.edges) {
            for (const auto i : e.vertices)
                w.U32(i);
            w.Float(e.compliance);
        }
        w.U32(static_cast<uint32_t>(b.mesh.tetrahedra.size()));
        for (const auto &t : b.mesh.tetrahedra) {
            for (const auto i : t.vertices)
                w.U32(i);
            w.Float(t.compliance);
        }
        w.U32(static_cast<uint32_t>(b.mesh.faces.size()));
        for (const auto &f : b.mesh.faces)
            for (const auto i : f)
                w.U32(i);
        w.U32(static_cast<uint32_t>(b.rootFollow.size()));
        for (const auto &follow : b.rootFollow) {
            w.U32(follow.vertex);
            w.Float(follow.compliance);
        }
        if (data.version >= 2)
            w.U32(static_cast<uint32_t>(data.normalPolicy));
        if (w.bytes.size() > MAX_SOFT_BODY_PRODUCT_BYTES)
            return Invalid("Soft body product is too large");
        out = std::move(w.bytes);
        return {};
    }

    phy::PhysicsStatus DecodeSoftBodyAsset(std::span<const uint8_t> bytes, SoftBodyAssetData &out)
    {
        if (bytes.size() > MAX_SOFT_BODY_PRODUCT_BYTES)
            return Invalid("Soft body product is too large");
        Reader            r{bytes};
        SoftBodyAssetData next;
        auto             &b     = next.body;
        uint32_t          magic = 0, sleeping = 0, count = 0;
        if (!r.U32(magic) || magic != MAGIC || !r.U32(next.version) || (next.version != 1 && next.version != 2) || !r.Id(next.mesh) ||
            !r.Id(next.skin) || !r.Id(next.skeleton) || !r.U64(next.skeletonLayout) || !r.U32(b.filter.group) || !r.U32(b.filter.mask) ||
            !r.U32(b.solverIterations) || !r.Float(b.pressure) || !r.Float(b.gravityFactor) || !r.Float(b.linearDamping) ||
            !r.Float(b.maxLinearVelocity) || !r.Float(b.vertexRadius) || !r.Float(b.friction) || !r.Float(b.restitution) || !r.U32(sleeping) ||
            sleeping > 1 || !r.Count(count, MAX_SOFT_BODY_PARTICLES))
            return Invalid("Invalid or truncated soft body product header");
        b.allowSleeping = sleeping != 0;
        b.mesh.vertices.resize(count);
        next.controlIds.resize(count);
        for (size_t i = 0; i < count; ++i) {
            auto    &v    = b.mesh.vertices[i];
            uint32_t mode = 0;
            if (!r.Vec(v.position) || !r.Vec(v.velocity) || !r.Float(v.inverseMass) || !r.U32(mode) ||
                mode >= static_cast<uint32_t>(phy::SoftBodyAttachmentMode::END) || !r.Text(next.controlIds[i]))
                return Invalid("Invalid soft body vertex record");
            v.mode = static_cast<phy::SoftBodyAttachmentMode>(mode);
        }
        if (!r.Count(count, MAX_CONSTRAINTS) || count > (bytes.size() - r.cursor) / 12)
            return Invalid("Invalid soft body edge count");
        b.mesh.edges.resize(count);
        for (auto &e : b.mesh.edges)
            if (!r.U32(e.vertices[0]) || !r.U32(e.vertices[1]) || !r.Float(e.compliance))
                return Invalid("Truncated soft body edges");
        if (!r.Count(count, MAX_CONSTRAINTS) || count > (bytes.size() - r.cursor) / 20)
            return Invalid("Invalid soft body tetrahedron count");
        b.mesh.tetrahedra.resize(count);
        for (auto &t : b.mesh.tetrahedra)
            if (!r.U32(t.vertices[0]) || !r.U32(t.vertices[1]) || !r.U32(t.vertices[2]) || !r.U32(t.vertices[3]) || !r.Float(t.compliance))
                return Invalid("Truncated soft body tetrahedra");
        if (!r.Count(count, MAX_CONSTRAINTS) || count > (bytes.size() - r.cursor) / 12)
            return Invalid("Invalid soft body face count");
        b.mesh.faces.resize(count);
        for (auto &f : b.mesh.faces)
            if (!r.U32(f[0]) || !r.U32(f[1]) || !r.U32(f[2]))
                return Invalid("Truncated soft body faces");
        if (!r.Count(count, static_cast<uint32_t>(b.mesh.vertices.size())) || count > (bytes.size() - r.cursor) / 8)
            return Invalid("Invalid soft body root follow count");
        b.rootFollow.resize(count);
        for (auto &follow : b.rootFollow)
            if (!r.U32(follow.vertex) || !r.Float(follow.compliance))
                return Invalid("Truncated soft body root follow constraints");
        if (next.version >= 2) {
            uint32_t policy = 0;
            if (!r.U32(policy) || policy >= static_cast<uint32_t>(SoftBodyNormalPolicy::END))
                return Invalid("Invalid or truncated soft body normal policy");
            next.normalPolicy = static_cast<SoftBodyNormalPolicy>(policy);
        }
        if (r.cursor != bytes.size())
            return Invalid("Trailing soft body product data");
        if (const auto status = ValidateSoftBodyAsset(next); !status)
            return status;
        out = std::move(next);
        return {};
    }

    void RegisterSoftBodyAssets()
    {
        auto *context = SerializationContext::Get();
        context->Register<SoftBodyComponentData>("SoftBodyComponentData").Member<&SoftBodyComponentData::rig>("rig");
        const bool registered = ComponentFactory::Get()->RegisterPersistentDataType(Uuid::CreateWithSeed(Fnv1a32("SoftBodyComponent")),
                                                                                    TypeInfoObj<SoftBodyComponentData>::Get()->RtInfo());
        SKY_ASSERT(registered);
        AssetManager::Get()->RegisterAssetHandler(SOFT_BODY_ASSET_TYPE, new SoftBodyHandler);
    }
} // namespace sky::softbody
