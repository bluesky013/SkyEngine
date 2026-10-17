//
// Created on 2026/10/17.
//

#include <aurora/adaptor/renderer/UiRenderFeature.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace sky::aurora {
    namespace {
        constexpr uint32_t           UI_VERSION   = 2;
        constexpr uint32_t           MAX_VERTICES = 65536, MAX_INDICES = 196608, MAX_DRAWS = 4096;
        std::optional<RendererError> ValidateSnapshot(const RenderUiSnapshot &snapshot)
        {
            if (!snapshot.output.IsValid() || snapshot.vertices.size() > MAX_VERTICES || snapshot.indices.size() > MAX_INDICES ||
                snapshot.draws.size() > MAX_DRAWS)
                return RendererError{RendererErrorCode::INVALID_PACKET, "Invalid UI snapshot budget or output"};
            for (const auto &vertex : snapshot.vertices)
                if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.u) || !std::isfinite(vertex.v) ||
                    !std::isfinite(vertex.radius) || vertex.radius < 0 || vertex.radius > 131072 ||
                    !std::all_of(vertex.shape.begin(), vertex.shape.end(),
                                 [](float value) { return std::isfinite(value) && std::abs(value) <= 131072; }) ||
                    std::abs(vertex.x) > 16 || std::abs(vertex.y) > 16 || !std::all_of(vertex.color.begin(), vertex.color.end(), [](float value) {
                        return std::isfinite(value) && value >= 0 && value <= 1;
                    }))
                    return RendererError{RendererErrorCode::INVALID_PACKET, "Invalid UI vertex"};
            for (const auto index : snapshot.indices)
                if (index >= snapshot.vertices.size())
                    return RendererError{RendererErrorCode::INVALID_PACKET, "UI index exceeds owned vertices"};
            for (const auto &draw : snapshot.draws)
                if ((draw.image.product ? (!draw.image.revision || !draw.image.deviceEpoch || draw.texture.viewport.IsValid())
                                        : (!draw.texture.viewport.IsValid() || draw.texture.viewport == snapshot.output)) ||
                    !draw.indexCount || draw.indexCount % 3 || draw.firstIndex > snapshot.indices.size() ||
                    draw.indexCount > snapshot.indices.size() - draw.firstIndex ||
                    !std::all_of(draw.clip.begin(), draw.clip.end(), [](float value) { return std::isfinite(value) && value >= 0 && value <= 1; }) ||
                    draw.clip[0] > draw.clip[2] || draw.clip[1] > draw.clip[3])
                    return RendererError{RendererErrorCode::INVALID_PACKET, "Invalid UI draw or feedback reference"};
            return {};
        }
        template <typename T>
        void Append(std::vector<uint8_t> &bytes, const T &value)
        {
            const auto offset = bytes.size();
            bytes.resize(offset + sizeof(T));
            std::memcpy(bytes.data() + offset, &value, sizeof(T));
        }
        std::variant<RendererError, RenderUiSnapshot> Decode(const FrameExtensionPayload &payload)
        {
            if (payload.outputs.size() != 1 || payload.bytes.size() < 16)
                return RendererError{RendererErrorCode::INVALID_PACKET, "UI payload header is missing"};
            std::array<uint32_t, 4> header;
            std::memcpy(header.data(), payload.bytes.data(), sizeof(header));
            const uint64_t size = 16 + uint64_t(header[1]) * 52 + uint64_t(header[2]) * 4 + uint64_t(header[3]) * 44;
            if (header[0] != UI_VERSION || header[1] > MAX_VERTICES || header[2] > MAX_INDICES || header[3] > MAX_DRAWS ||
                size != payload.bytes.size())
                return RendererError{RendererErrorCode::INVALID_PACKET, "UI payload size or version is invalid"};
            size_t     offset = 16;
            const auto read   = [&]<typename T>(T &value) {
                std::memcpy(&value, payload.bytes.data() + offset, sizeof(T));
                offset += sizeof(T);
            };
            RenderUiSnapshot snapshot;
            snapshot.output = payload.outputs.front();
            snapshot.vertices.resize(header[1]);
            snapshot.indices.resize(header[2]);
            snapshot.draws.resize(header[3]);
            for (auto &vertex : snapshot.vertices) {
                read(vertex.x);
                read(vertex.y);
                read(vertex.u);
                read(vertex.v);
                read(vertex.color);
                read(vertex.shape);
                read(vertex.radius);
            }
            for (auto &index : snapshot.indices)
                read(index);
            for (auto &draw : snapshot.draws) {
                read(draw.texture.viewport.index);
                read(draw.texture.viewport.generation);
                read(draw.texture.revision);
                read(draw.firstIndex);
                read(draw.indexCount);
                read(draw.clip);
                uint32_t resource = UINT32_MAX;
                read(resource);
                if (resource != UINT32_MAX) {
                    if (resource >= payload.resources.size())
                        return RendererError{RendererErrorCode::INVALID_PACKET, "UI image reference is missing"};
                    draw.image = payload.resources[resource];
                }
            }
            if (auto error = ValidateSnapshot(snapshot))
                return *error;
            return snapshot;
        }
        class UiRenderFeature final : public IFrameGraphFeature {
        public:
            UiRenderFeature(Name name, RenderMaterialData code) : name(name), code(std::move(code))
            {
            }
            Name GetName() const override
            {
                return name;
            }
            std::optional<RendererError> Validate(const FrameExtensionPayload &payload) const override
            {
                const auto decoded = Decode(payload);
                if (const auto *error = std::get_if<RendererError>(&decoded))
                    return *error;
                return {};
            }
            void OnRetired() override
            {
                shader.Reset(nullptr);
                sampler.Reset(nullptr);
            }
            FrameOperationResult BuildFrame(FrameGraphBuildContext &context, const FrameExtensionPayload &payload) override
            {
                auto decoded = Decode(payload);
                if (const auto *error = std::get_if<RendererError>(&decoded))
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT, error->message};
                auto       snapshot = std::move(std::get<RenderUiSnapshot>(decoded));
                const auto output   = context.GetOutput(snapshot.output);
                if (!output)
                    return FrameOperationComplete{};
                FrameRasterPass pass;
                pass.color         = output->color;
                pass.load          = output->initialized ? LoadOp::LOAD : LoadOp::CLEAR;
                const auto resolve = [&](const RenderUiDraw &draw) {
                    return draw.image.product ? context.ResolveImage(draw.image) : context.ResolveViewOutput(draw.texture);
                };
                std::erase_if(snapshot.draws, [&](const auto &draw) { return !resolve(draw); });
                if (snapshot.draws.empty())
                    return context.Raster(std::move(pass));
                if (!shader) {
                    auto created = context.CreateShader(code);
                    if (const auto *error = std::get_if<FrameError>(&created))
                        return *error;
                    shader = std::move(std::get<ShaderPtr>(created));
                }
                if (!sampler) {
                    Sampler::Descriptor desc;
                    desc.magFilter = desc.minFilter = Filter::LINEAR;
                    desc.mipmapMode                 = MipFilter::NEAREST;
                    desc.addressModeU = desc.addressModeV = WrapMode::CLAMP_TO_EDGE;
                    auto created                          = context.CreateSampler(desc);
                    if (const auto *error = std::get_if<FrameError>(&created))
                        return *error;
                    sampler = std::move(std::get<SamplerPtr>(created));
                }
                const auto &matrix    = output->logicalToClip;
                const auto  transform = [&](float x, float y) {
                    return std::array{matrix.v[0] * (x * 2 - 1) + matrix.v[4] * (y * 2 - 1), matrix.v[1] * (x * 2 - 1) + matrix.v[5] * (y * 2 - 1)};
                };
                for (auto &vertex : snapshot.vertices) {
                    const auto position = transform(vertex.x, vertex.y);
                    vertex.x            = position[0];
                    vertex.y            = position[1];
                }
                auto vertices = context.Upload(std::as_bytes(std::span(snapshot.vertices)), BufferUsageFlagBit::VERTEX);
                if (const auto *error = std::get_if<FrameError>(&vertices))
                    return *error;
                auto indices = context.Upload(std::as_bytes(std::span(snapshot.indices)), BufferUsageFlagBit::INDEX);
                if (const auto *error = std::get_if<FrameError>(&indices))
                    return *error;
                for (const auto &command : snapshot.draws) {
                    const auto a = transform(command.clip[0], command.clip[1]), b = transform(command.clip[2], command.clip[3]);
                    const auto pixelY = [&](float y) {
                        return context.GetInputs().lease.context.backend == API::DX12 ? (1 - y) * .5f : (y + 1) * .5f;
                    };
                    const auto minX = static_cast<int32_t>(std::floor((std::min(a[0], b[0]) + 1) * .5f * output->output.extent.width));
                    const auto minY = static_cast<int32_t>(std::floor(std::min(pixelY(a[1]), pixelY(b[1])) * output->output.extent.height));
                    const auto maxX = static_cast<int32_t>(std::ceil((std::max(a[0], b[0]) + 1) * .5f * output->output.extent.width));
                    const auto maxY = static_cast<int32_t>(std::ceil(std::max(pixelY(a[1]), pixelY(b[1])) * output->output.extent.height));
                    if (maxX <= minX || maxY <= minY)
                        continue;
                    RasterDraw draw;
                    draw.shader     = shader;
                    draw.blend      = code.blend;
                    draw.scissor    = {{minX, minY}, {static_cast<uint32_t>(maxX - minX), static_cast<uint32_t>(maxY - minY)}};
                    draw.vertices   = {{std::get<RasterBufferRange>(vertices), sizeof(RenderUiVertex)}};
                    draw.attributes = {{0, 0, 0, Format::F_RG32, "POSITION", 0},
                                       {1, 0, 8, Format::F_RG32, "TEXCOORD", 0},
                                       {2, 0, 16, Format::F_RGBA32, "COLOR", 0},
                                       {3, 0, 32, Format::F_RGBA32, "TEXCOORD", 1},
                                       {4, 0, 48, Format::F_R32, "TEXCOORD", 2}};
                    draw.indexed =
                        RasterIndexedDraw{std::get<RasterBufferRange>(indices), IndexType::U32, {command.indexCount, 1, command.firstIndex}};
                    draw.images   = {{0, 0, *resolve(command)}};
                    draw.samplers = {{0, 1, sampler}};
                    pass.draws.push_back(std::move(draw));
                }
                return context.Raster(std::move(pass));
            }

        private:
            Name               name;
            RenderMaterialData code;
            ShaderPtr          shader;
            SamplerPtr         sampler;
        };
    } // namespace
    std::variant<RendererError, FrameExtensionPayload> EncodeRenderUiSnapshot(Name feature, const RenderUiSnapshot &snapshot)
    {
        if (auto error = ValidateSnapshot(snapshot))
            return *error;
        FrameExtensionPayload result;
        result.feature = feature;
        result.outputs = {snapshot.output};
        Append(result.bytes, std::array{UI_VERSION, static_cast<uint32_t>(snapshot.vertices.size()), static_cast<uint32_t>(snapshot.indices.size()),
                                        static_cast<uint32_t>(snapshot.draws.size())});
        for (const auto &vertex : snapshot.vertices) {
            Append(result.bytes, vertex.x);
            Append(result.bytes, vertex.y);
            Append(result.bytes, vertex.u);
            Append(result.bytes, vertex.v);
            Append(result.bytes, vertex.color);
            Append(result.bytes, vertex.shape);
            Append(result.bytes, vertex.radius);
        }
        for (const auto index : snapshot.indices)
            Append(result.bytes, index);
        for (const auto &draw : snapshot.draws) {
            Append(result.bytes, draw.texture.viewport.index);
            Append(result.bytes, draw.texture.viewport.generation);
            Append(result.bytes, draw.texture.revision);
            Append(result.bytes, draw.firstIndex);
            Append(result.bytes, draw.indexCount);
            Append(result.bytes, draw.clip);
            uint32_t resource = UINT32_MAX;
            if (draw.image.product) {
                const auto found = std::find(result.resources.begin(), result.resources.end(), draw.image);
                resource         = static_cast<uint32_t>(std::distance(result.resources.begin(), found));
                if (found == result.resources.end())
                    result.resources.push_back(draw.image);
            }
            Append(result.bytes, resource);
        }
        return result;
    }
    std::string_view RenderUiShaderSource()
    {
        return R"(
            Texture2D<float4> image : register(t0, space0);
            SamplerState imageSampler : register(s1, space0);
            struct Varying { float4 position : SV_Position; float2 uv : TEXCOORD0; float4 color : COLOR0; float4 shape : TEXCOORD1; float radius : TEXCOORD2; };
            Varying VSMain(float2 position : POSITION, float2 uv : TEXCOORD0, float4 color : COLOR0, float4 shape : TEXCOORD1, float radius : TEXCOORD2) {
                Varying result;
                result.position = float4(position, 0, 1); result.uv = uv; result.color = color; result.shape = shape; result.radius = radius;
                return result;
            }
            float4 FSMain(Varying input) : SV_Target {
                float4 color = image.Sample(imageSampler, input.uv) * input.color;
                if (input.radius > 0) {
                    float2 q = abs(input.shape.xy) - input.shape.zw + input.radius;
                    float d = length(max(q, 0)) + min(max(q.x, q.y), 0) - input.radius;
                    color.a *= saturate(0.5 - d / max(fwidth(d), 0.0001));
                }
                return color;
            }
        )";
    }
    std::variant<RendererError, std::shared_ptr<IFrameGraphFeature>> CreateUiRenderFeature(Name name, RenderMaterialData code)
    {
        if (name.Empty() || code.vertexCode.empty() || code.fragmentCode.empty() || code.vertexEntry.empty() || code.fragmentEntry.empty())
            return RendererError{RendererErrorCode::INVALID_ARGUMENT, "UI feature requires a name and compiled shader"};
        const auto &resources = code.reflection.resources;
        if (code.vertexCode.size() > 16 * 1024 * 1024 || code.fragmentCode.size() > 16 * 1024 * 1024 || resources.size() != 2 ||
            !code.reflection.blocks.empty() || !code.reflection.pushConstants.empty())
            return RendererError{RendererErrorCode::INVALID_ARGUMENT, "UI shader exceeds its code or binding budget"};
        bool image = false, sampler = false;
        for (const auto &resource : resources) {
            if (resource.set || resource.count != 1 || resource.descriptorArray)
                return RendererError{RendererErrorCode::INVALID_ARGUMENT, "UI shader requires scalar set zero bindings"};
            image |= resource.binding == 0 && resource.type == ShaderResourceType::SAMPLED_IMAGE &&
                     resource.texture == ShaderTextureType{ShaderTextureDimension::D2, ShaderScalarType::FLOAT, 4, false, false};
            sampler |= resource.binding == 1 && resource.type == ShaderResourceType::SAMPLER && resource.sampler == ShaderSamplerType::REGULAR;
        }
        if (!image || !sampler)
            return RendererError{RendererErrorCode::INVALID_ARGUMENT, "UI shader requires Texture2D at zero and sampler at one"};
        return std::shared_ptr<IFrameGraphFeature>(std::make_shared<UiRenderFeature>(name, std::move(code)));
    }
} // namespace sky::aurora
