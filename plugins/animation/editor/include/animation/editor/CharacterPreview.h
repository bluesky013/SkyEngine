#pragma once

#include <animation/IAnimationClipSource.h>
#include <animation/IAnimationInstanceControl.h>
#include <animation/IAnimationSkeletonSource.h>
#include <animation/assets/GraphAsset.h>
#include <deque>
#include <editor/core/asset/AssetPreviewProvider.h>

namespace sky::animation::editor {
    struct CharacterPreviewSettings {
        Uuid  mesh, skin, skeleton, material;
        bool  referencePose = false, loop = true, showBones = true, showBounds = true;
        float rate = 1.f, scrubSeconds = 0.f;
        bool  operator==(const CharacterPreviewSettings &) const = default;
    };
    void                        RegisterCharacterPreviewSettings();
    std::shared_ptr<const void> RetainCharacterPreviewModules();
    AnimationStatus             ApplyImportedCharacterDefaults(CharacterPreviewSettings &settings);

    class CharacterPreview {
    public:
        CharacterPreview(Uuid asset, std::string type, std::shared_ptr<const void> code);
        ~CharacterPreview();
        void                            Configure(CharacterPreviewSettings value);
        void                            Tick(float delta);
        void                            Resize(uint32_t width, uint32_t height);
        void                            Control(AnimationControlOp op, float value = 0.f);
        void                            SetParameter(const Uuid &id, AnimationParam value);
        AnimationStatus                 FocusCharacter();
        void                            Close();
        const CharacterPreviewSettings &Settings() const
        {
            return settings;
        }
        const AnimationInstanceStatus &Playback() const
        {
            return playback;
        }
        const AnimationSourceResult<Skeleton> &Rig() const
        {
            return rig;
        }
        const std::vector<Matrix4> &BoneModels() const
        {
            return boneModels;
        }
        const std::string &Diagnostic() const
        {
            return inputDiagnostic.empty() ? diagnostic : inputDiagnostic;
        }
        const sky::editor::AssetPreviewStatus &Output() const
        {
            return output;
        }
        sky::editor::EditorCamera &Camera()
        {
            return camera;
        }
        bool IsGraph() const
        {
            return type == "AnimationGraph";
        }
        bool CanPlay() const
        {
            return !settings.referencePose && playback.state == AnimationSourceState::Ready;
        }
        const std::shared_ptr<const void> &CodeLease() const
        {
            return code;
        }
        uint32_t selectedBone = 0;
        void     ReportError(std::string message)
        {
            inputDiagnostic = std::move(message);
        }

    private:
        bool                                             BuildWorld();
        void                                             ResolveRig();
        Uuid                                             asset;
        std::string                                      type;
        std::shared_ptr<const void>                      code;
        WorldPtr                                         world;
        Uuid                                             actor;
        CharacterPreviewSettings                         settings;
        bool                                             rebuild      = true;
        bool                                             frameOnReady = true;
        uint32_t                                         width = 320, height = 240;
        sky::editor::AssetPreviewHandle                  session;
        sky::editor::AssetPreviewStatus                  output;
        sky::editor::EditorCamera                        camera;
        AnimationInstanceStatus                          playback;
        AnimationSourceRequest<Skeleton>                 rigRequest;
        AnimationSourceRequest<AnimationClip>            clipRequest;
        std::shared_ptr<Asset<GraphAsset>>               graphAsset;
        AnimationSourceResult<Skeleton>                  rig;
        Uuid                                             requestedRig;
        uint64_t                                         rigSerial = 0, graphSerial = 0;
        std::vector<Matrix4>                             boneModels;
        std::deque<std::pair<AnimationControlOp, float>> controls;
        std::vector<std::pair<Uuid, AnimationParam>>     parameters;
        std::string                                      diagnostic;
        std::string                                      inputDiagnostic;
    };
} // namespace sky::animation::editor
