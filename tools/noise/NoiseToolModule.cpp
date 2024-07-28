//
// Created by blues on 2023/9/28.
//
#pragma once

#include <framework/window/IWindowEvent.h>
#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/window/NativeWindow.h>
#include <framework/asset/AssetManager.h>
#include <framework/world/World.h>
#include <framework/world/Actor.h>
#include <framework/world/TransformComponent.h>

#include <render/Renderer.h>
#include <render/adaptor/RenderSceneProxy.h>
#include <render/adaptor/pipeline/DefaultForward.h>
#include <render/adaptor/assets/MeshAsset.h>
#include <render/adaptor/components/StaticMeshComponent.h>
#include <render/adaptor/components/CameraComponent.h>
#include <render/adaptor/Util.h>
#include <render/RHI.h>

#include <imgui/ImGuiFeatureProcessor.h>

#include <core/math/Math.h>
#include <core/math/MathUtil.h>
#include <core/math/Random.h>
#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <core/math/Vector4.h>

#include <NoiseGenerator/ProjectRoot.h>

#include <PerlinNoise.hpp>
#include <memory>


namespace sky {
    class RenderScene;
    class RenderWindow;

    enum class NoiseType {
        PERLIN,
        WORLEY,
        PERLIN_WORLEY,
        NUM
    };

    enum class NoiseDimension {
        NOISE_2D,
        NOISE_3D
    };

    Vector2 Random2(const Vector2 &v)
    {
        Vector2 res(sin(v.Dot(Vector2(127.1f, 311.7f)) * 43758.5453f),
                    sin(v.Dot(Vector2(269.5f, 183.3f)) * 43758.5453f));
        return Fract(res / 2.f + Vector2(0.5f));
    }

    Vector3 Random3(const Vector3 &v)
    {
        Vector3 res(sin(v.Dot(Vector3(127.1f, 311.7f, 241.5)) * 43758.5453f),
                    sin(v.Dot(Vector3(269.5f, 183.3f, 385.1)) * 43758.5453f),
                    sin(v.Dot(Vector3(521.1f, 117.3f, 421.3)) * 43758.5453f));
        return Fract(res / 2.f + Vector3(0.5f));
    }

    float Remap(float originalValue, float originalMin, float originalMax, float newMin, float newMax)
    {
        return newMin + (((originalValue - originalMin) / (originalMax - originalMin)) * (newMax - newMin));
    }

    float NoiseWorley(const Vector2 &v, float s)
    {
        float min = 1000.0f;

        for (int x = -1; x <= 1; ++x) {
            for (int y = -1; y <= 1; ++y) {
                Vector2 off {static_cast<float>(x), static_cast<float>(y)};
                Vector2 id = Floor(v / s) + off;
                float d = (Random2(id) + off - Fract(v / s)).Length();
                min = std::min(d, min);
            }
        }
        return std::clamp(min, 0.f, 1.f);
    }

    float NoiseWorley3(const Vector3 &v, float s)
    {
        float min = 1000.0f;

        for (int x = -1; x <= 1; ++x) {
            for (int y = -1; y <= 1; ++y) {
                for (int z = -1; z <= 1; ++z) {
                    Vector3 off{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
                    Vector3 id = Floor(v / s) + off;
                    float   d  = (Random3(id) + off - Fract(v / s)).Length();
                    min = std::min(d, min);
                }
            }
        }
        return std::clamp(min, 0.f, 1.f);
    }

    class SubWidget {
    public:
        SubWidget() = default;
        virtual ~SubWidget() = default;

        virtual void Execute() = 0;
        virtual void Generate2D(uint32_t extent) = 0;
        virtual void Generate3D(uint32_t extent) = 0;
    };

    class PerlinNoiseWidget : public SubWidget {
    public:
        explicit PerlinNoiseWidget(StaticMeshRenderer *mesh) : seed(113344U), meshRenderer(mesh) {
            auto mat = AssetManager::Get()->LoadAsset<Material>("materials/noise.mat")->CreateInstance();
            material = std::make_shared<MaterialInstance>();
            material->SetMaterial(mat);
            material->SetValue("baseColor", Vector4(1, 1, 1, 1));
        }
        ~PerlinNoiseWidget() override = default;

        void Execute() override
        {
            ImGui::Text("Perlin Noise Setup");
            ImGui::NewLine();
            ImGui::SliderFloat("Factor", &factor, 0.001f, 1.f);
            ImGui::SliderInt("Octave", &octave, 1, 5);
            ImGui::InputScalar("Seed", ImGuiDataType_U32, &seed);
            ImGui::SameLine();
            if (ImGui::Button("Get")) {
                Random::Gen(&seed, sizeof(uint32_t));
            }
        }

        void Generate2D(uint32_t extent) override
        {
            if (!texture || !texture->CheckExtent(extent, extent)) {
                texture = std::make_shared<Texture2D>();
                texture->Init(rhi::PixelFormat::R16_UNORM, extent, extent, 1);
            }

            const siv::PerlinNoise perlin{ siv::PerlinNoise::seed_type(seed) };
            std::vector<uint16_t> data(extent * extent, 0);
            for (uint32_t i = 0; i < extent; i ++) {
                for (uint32_t j = 0; j < extent; j++) {
                    const double noise = perlin.octave2D_01(static_cast<float>(i) * factor, static_cast<float>(j) * factor, octave);
                    data[i * extent + j] = static_cast<uint32_t>(noise * 65536);
                }
            }

            rhi::Queue *queue = RHI::Get()->GetDevice()->GetQueue(rhi::QueueType::TRANSFER);
            auto handle = texture->Upload(reinterpret_cast<uint8_t *>(data.data()), data.size() * sizeof(uint16_t), *queue);
            queue->Wait(handle);
            material->SetTexture("mainColor", texture);
            material->Upload();

            meshRenderer->SetMaterial(material, 0);
        }

        void Generate3D(uint32_t extent) override
        {
            if (!texture3D || !texture3D->CheckExtent(extent, extent, extent)) {
                texture3D = std::make_shared<Texture3D>();
                texture3D->Init(rhi::PixelFormat::R32_SFLOAT, extent, extent, extent);
            }

            const siv::PerlinNoise perlin{ siv::PerlinNoise::seed_type(seed) };
            std::vector<float> data(extent * extent * extent, 0);
            for (uint32_t i = 0; i < extent; i ++) {
                for (uint32_t j = 0; j < extent; j++) {
                    for (uint32_t k = 0; k < extent; k++) {
                        const auto noise = perlin.octave3D_01(static_cast<float>(i) * factor,
                                                                  static_cast<float>(j) * factor,
                                                                  static_cast<float>(k) * factor,
                                                                  octave);
                        data[i * extent * extent + j * extent + k] = static_cast<float>(noise);
                    }
                }
            }

            rhi::Queue *queue = RHI::Get()->GetDevice()->GetQueue(rhi::QueueType::TRANSFER);
            auto handle = texture3D->Upload(reinterpret_cast<uint8_t *>(data.data()), data.size() * sizeof(float), *queue);
            queue->Wait(handle);
        }

    private:
        uint32_t seed;
        int32_t octave = 1;
        float factor = 0.5f;
        StaticMeshRenderer *meshRenderer;
        RDTexture2DPtr texture;
        RDTexture3DPtr texture3D;
        RDMaterialInstancePtr material;
    };

    class WorleyNoiseWidget : public SubWidget {
    public:
        explicit WorleyNoiseWidget(StaticMeshRenderer *mesh) : seed(113344), meshRenderer(mesh) {
            auto mat = AssetManager::Get()->LoadAsset<Material>("materials/noise.mat")->CreateInstance();
            material = std::make_shared<MaterialInstance>();
            material->SetMaterial(mat);
            material->SetValue("baseColor", Vector4(1, 1, 1, 1));
        }
        ~WorleyNoiseWidget() override = default;

        void Execute() override
        {
            ImGui::Text("Perlin Noise Setup");
            ImGui::NewLine();
            ImGui::SliderScalar("Frequency", ImGuiDataType_U32, &frequency, &freqMin, &freqMax);
            ImGui::InputScalar("Seed", ImGuiDataType_S32, &seed);
            ImGui::SameLine();
            if (ImGui::Button("Get")) {
                Random::Gen(&seed, sizeof(uint32_t));
            }
        }

        void Generate2D(uint32_t extent) override
        {
            if (!texture || !texture->CheckExtent(extent, extent)) {
                texture = std::make_shared<Texture2D>();
                texture->Init(rhi::PixelFormat::R32_SFLOAT, extent, extent, 1);
            }

            std::vector<float> data(extent * extent, 0);

            auto s = static_cast<float>(extent) / static_cast<float>(frequency);
            for (uint32_t i = 0; i < extent; i ++) {
                for (uint32_t j = 0; j < extent; j++) {
                    data[i * extent + j] = 1.f - NoiseWorley(Vector2(static_cast<float>(j), static_cast<float>(i)), s);
                }
            }

            rhi::Queue *queue = RHI::Get()->GetDevice()->GetQueue(rhi::QueueType::TRANSFER);
            auto handle = texture->Upload(reinterpret_cast<uint8_t *>(data.data()), data.size() * sizeof(float ), *queue);
            queue->Wait(handle);
            material->SetTexture("mainColor", texture);
            material->Upload();

            meshRenderer->SetMaterial(material, 0);
        }

        void Generate3D(uint32_t extent) override
        {
            if (!texture3D || !texture3D->CheckExtent(extent, extent, extent)) {
                texture3D = std::make_shared<Texture3D>();
                texture3D->Init(rhi::PixelFormat::R32_SFLOAT, extent, extent, extent);
            }

            std::vector<float> data(extent * extent * extent, 0);

            auto s = static_cast<float>(extent) / static_cast<float>(frequency);
            for (uint32_t i = 0; i < extent; i ++) {
                for (uint32_t j = 0; j < extent; j++) {
                    for (uint32_t k = 0; k < extent; k++) {
                        data[i * extent * extent + j * extent + k] =
                            1.f - NoiseWorley3(Vector3(static_cast<float>(k),
                                                       static_cast<float>(j),
                                                       static_cast<float>(i)), s);
                    }
                }
            }

            rhi::Queue *queue = RHI::Get()->GetDevice()->GetQueue(rhi::QueueType::TRANSFER);
            auto handle = texture3D->Upload(reinterpret_cast<uint8_t *>(data.data()), data.size() * sizeof(float), *queue);
            queue->Wait(handle);
        }

    private:
        int seed;
        uint32_t freqMin = 1;
        uint32_t freqMax = 20;
        uint32_t frequency = 1;
        StaticMeshRenderer *meshRenderer;
        RDTexture2DPtr texture;
        RDTexture3DPtr texture3D;
        RDMaterialInstancePtr material;
    };

    class PerlinWorleyNoiseWidget : public SubWidget {
    public:
        explicit PerlinWorleyNoiseWidget(StaticMeshRenderer *mesh) : seed(113344), meshRenderer(mesh) {
            auto mat = AssetManager::Get()->LoadAsset<Material>("materials/noise.mat")->CreateInstance();
            material = std::make_shared<MaterialInstance>();
            material->SetMaterial(mat);
            material->SetValue("baseColor", Vector4(1, 1, 1, 1));
        }
        ~PerlinWorleyNoiseWidget() override = default;

        void Execute() override
        {
            ImGui::Text("Perlin Noise Setup");
            ImGui::NewLine();
            ImGui::SliderFloat("Factor", &factor, 0.001f, 1.f);
            ImGui::SliderInt("Octave", &octave, 1, 5);
            ImGui::SliderScalar("Frequency", ImGuiDataType_U32, &frequency, &freqMin, &freqMax);
            ImGui::InputScalar("Seed", ImGuiDataType_S32, &seed);
            ImGui::SameLine();
            if (ImGui::Button("Get")) {
                Random::Gen(&seed, sizeof(uint32_t));
            }
        }

        void Generate2D(uint32_t extent) override
        {
            if (!texture || !texture->CheckExtent(extent, extent)) {
                texture = std::make_shared<Texture2D>();
                texture->Init(rhi::PixelFormat::R32_SFLOAT, extent, extent, 1);
            }

            const siv::PerlinNoise perlin{ siv::PerlinNoise::seed_type(seed) };
            std::vector<float> data(extent * extent, 0);

            auto s = static_cast<float>(extent) / static_cast<float>(frequency);
            for (uint32_t i = 0; i < extent; i ++) {
                for (uint32_t j = 0; j < extent; j++) {
                    float w = 1.f - NoiseWorley(Vector2(static_cast<float>(j), static_cast<float>(i)), s);
                    float p = perlin.octave2D_01(static_cast<float>(j) * factor, static_cast<float>(i) * factor, octave);
                    data[i * extent + j] = Remap(p, w, 1.0f, 0.f, 1.0f);
                }
            }

            rhi::Queue *queue = RHI::Get()->GetDevice()->GetQueue(rhi::QueueType::TRANSFER);
            auto handle = texture->Upload(reinterpret_cast<uint8_t *>(data.data()), data.size() * sizeof(float ), *queue);
            queue->Wait(handle);
            material->SetTexture("mainColor", texture);
            material->Upload();

            meshRenderer->SetMaterial(material, 0);
        }

        void Generate3D(uint32_t extent) override
        {
            if (!texture3D || !texture3D->CheckExtent(extent, extent, extent)) {
                texture3D = std::make_shared<Texture3D>();
                texture3D->Init(rhi::PixelFormat::R32_SFLOAT, extent, extent, extent);
            }

            const siv::PerlinNoise perlin{ siv::PerlinNoise::seed_type(seed) };
            std::vector<float> data(extent * extent * extent, 0);

            auto s = static_cast<float>(extent) / static_cast<float>(frequency);
            for (uint32_t i = 0; i < extent; i ++) {
                for (uint32_t j = 0; j < extent; j++) {
                    for (uint32_t k = 0; k < extent; k++) {
                        float w = 1.f - NoiseWorley3(Vector3(static_cast<float>(k),
                                                             static_cast<float>(j),
                                                             static_cast<float>(i)),s);
                        float p = perlin.octave3D_01(static_cast<float>(k) * factor,
                                                     static_cast<float>(j) * factor,
                                                     static_cast<float>(i) * factor,
                                                     octave);

                        data[i * extent * extent + j * extent + k] = Remap(p, w, 1.0f, 0.f, 1.0f);
                    }
                }
            }

            rhi::Queue *queue = RHI::Get()->GetDevice()->GetQueue(rhi::QueueType::TRANSFER);
            auto handle = texture3D->Upload(reinterpret_cast<uint8_t *>(data.data()), data.size() * sizeof(float), *queue);
            queue->Wait(handle);
        }

    private:
        int seed;
        int octave = 1;
        float factor = 0.5f;

        uint32_t freqMin = 1;
        uint32_t freqMax = 20;
        uint32_t frequency = 1;
        StaticMeshRenderer *meshRenderer;
        RDTexture2DPtr texture;
        RDTexture3DPtr texture3D;
        RDMaterialInstancePtr material;
    };

    class NoiseSetupWidget : public ImWidget {
    public:
        explicit NoiseSetupWidget(StaticMeshRenderer *renderer) {
            subWidgets.resize(static_cast<uint32_t>(NoiseType::NUM));
            subWidgets[static_cast<uint32_t>(NoiseType::PERLIN)] = std::make_unique<PerlinNoiseWidget>(renderer);
            subWidgets[static_cast<uint32_t>(NoiseType::WORLEY)] = std::make_unique<WorleyNoiseWidget>(renderer);
            subWidgets[static_cast<uint32_t>(NoiseType::PERLIN_WORLEY)] = std::make_unique<PerlinWorleyNoiseWidget>(renderer);
        }
        ~NoiseSetupWidget() override = default;

        void Execute(ImGuiContext *context) override
        {
            ImGui::SetCurrentContext(context);

            ImGui::Begin("Noise Setup");

            {
                const char* items[] = { "PERLIN", "WORLEY", "PERLIN_WORLEY" };
                ImGui::Combo("NoiseType", reinterpret_cast<int*>(&type), items, IM_ARRAYSIZE(items));

                ImGui::InputScalar("Extent", ImGuiDataType_U32, &extent);
            }

            {
                const char* items[] = { "2D", "3D" };
                ImGui::Combo("Dimension", reinterpret_cast<int*>(&dimension), items, IM_ARRAYSIZE(items));
            }

            ImGui::Separator();

            auto *widget = subWidgets[static_cast<uint32_t>(type)].get();
            if (widget != nullptr) {
                widget->Execute();
            }

            ImGui::Separator();

            if (ImGui::Button("Generate") && widget != nullptr){
                if (dimension == NoiseDimension::NOISE_2D) {
                    widget->Generate2D(extent);
                } else {
                    widget->Generate3D(extent);
                }
            }
            ImGui::End();
        }

    private:
        NoiseType type = NoiseType::PERLIN;
        NoiseDimension dimension = NoiseDimension::NOISE_2D;
        uint32_t extent = 128;

        std::vector<std::unique_ptr<SubWidget>> subWidgets;
    };

    class NoiseToolModule : public IModule, public IWindowEvent {
    public:
        NoiseToolModule()  = default;
        ~NoiseToolModule() override = default;

        bool Init() override
        {
            AssetManager::Get()->RegisterSearchPath(ENGINE_ROOT + "/assets");
            AssetManager::Get()->RegisterSearchPath(PROJECT_ROOT + "/assets");
            AssetManager::Get()->RegisterSearchPath(PROJECT_ROOT + "/cache");
            AssetManager::Get()->Reset(PROJECT_ROOT + "/assets.db");
            return true;
        }

        void Start() override
        {
            auto *renderer = Renderer::Get();
            const auto *nativeWindow = Interface<ISystemNotify>::Get()->GetApi()->GetViewport();
            window = renderer->CreateRenderWindow(nativeWindow->GetNativeHandle(), nativeWindow->GetWidth(), nativeWindow->GetHeight(), false);
            Event<IWindowEvent>::Connect(nativeWindow, this);

            world = std::make_unique<World>();
            sceneProxy = std::make_unique<RenderSceneProxy>();
            auto *renderScene = sceneProxy->GetRenderScene();
            auto *ppl = new DefaultForward();
            ppl->SetOutput(window);
            renderScene->SetPipeline(ppl);
            world->SetRenderScene(sceneProxy.get());

            meshObj = world->CreateGameObject("Plane");
            auto mesh = AssetManager::Get()->LoadAsset<Mesh>("plane/plane_mesh_0.mesh");
            auto *meshRenderer = meshObj->AddComponent<MeshRenderer>();
            meshRenderer->SetMesh(mesh);
            meshObj->GetComponent<TransformComponent>()->SetLocalRotation(Quaternion(90.f / 180.f * PI, VEC3_X));

            auto *gui = GetFeatureProcessor<ImGuiFeatureProcessor>(sceneProxy->GetRenderScene())->CreateGUIInstance();
            gui->BindNativeWindow(nativeWindow);
            gui->AddWidget<NoiseSetupWidget>(meshRenderer->GetRenderer());

            camera = world->CreateGameObject("MainCamera");
            auto *cc = camera->AddComponent<CameraComponent>();
            cc->Perspective(0.01f, 100.f, 45.f / 180.f * 3.14f);
            cc->SetAspect(window->GetWidth(), window->GetHeight());
            camera->GetComponent<TransformComponent>()->SetWorldTranslation(Vector3(0, 0, 5));
        }

        void Stop() override
        {
            world = nullptr;
            sceneProxy = nullptr;

            auto *renderer = Renderer::Get();
            renderer->DestroyRenderWindow(window);
            Event<IWindowEvent>::DisConnect(this);
        }

        void Tick(float delta) override
        {
            world->Tick(delta);
        }

        void OnWindowResize(uint32_t width, uint32_t height) override
        {
            if (window != nullptr) {
                window->Resize(width, height);
            }
        }
    private:
        RenderWindow *window = nullptr;

        std::unique_ptr<World> world;
        std::unique_ptr<RenderSceneProxy> sceneProxy;

        GameObject *meshObj = nullptr;
        GameObject *camera = nullptr;
    };

} // namespace sky

REGISTER_MODULE(sky::NoiseToolModule)