//
// Created by SkyEngine on 2025/3/7.
//

#include <render/adaptor/components/HeightFogComponent.h>
#include <render/adaptor/Util.h>
#include <framework/world/ComponentFactory.h>
#include <framework/serialization/PropertyCommon.h>

namespace sky {

    void HeightFogComponent::Reflect(SerializationContext *context)
    {
        context->Register<HeightFogData>("HeightFogData")
            .Member<&HeightFogData::fogColor>("FogColor")
            .Member<&HeightFogData::inscatterColor>("InscatterColor")
            .Member<&HeightFogData::fogDensity>("FogDensity")
            .Member<&HeightFogData::heightFalloff>("HeightFalloff")
            .Member<&HeightFogData::baseHeight>("BaseHeight")
            .Member<&HeightFogData::maxHeight>("MaxHeight")
            .Member<&HeightFogData::startDistance>("StartDistance");

        REGISTER_BEGIN(HeightFogComponent, context)
            REGISTER_MEMBER(FogColor,      SetFogColor,      GetFogColor)
            REGISTER_MEMBER(InscatterColor, SetInscatterColor, GetInscatterColor)
            REGISTER_MEMBER(FogDensity,    SetFogDensity,    GetFogDensity)
            REGISTER_MEMBER(HeightFalloff, SetHeightFalloff, GetHeightFalloff)
            REGISTER_MEMBER(BaseHeight,    SetBaseHeight,    GetBaseHeight)
            REGISTER_MEMBER(MaxHeight,     SetMaxHeight,     GetMaxHeight)
            REGISTER_MEMBER(StartDistance, SetStartDistance, GetStartDistance);
    }

    void HeightFogComponent::OnAttachToWorld()
    {
        fp = GetFeatureProcessor<HeightFogFeatureProcessor>(actor);
        if (fp != nullptr) {
            fp->SetFogColor(Vector4(data.fogColor.r, data.fogColor.g, data.fogColor.b, 0.f));
            fp->SetInscatterColor(Vector4(data.inscatterColor.r, data.inscatterColor.g, data.inscatterColor.b, 0.f));
            fp->SetFogDensity(data.fogDensity);
            fp->SetHeightFalloff(data.heightFalloff);
            fp->SetBaseHeight(data.baseHeight);
            fp->SetMaxHeight(data.maxHeight);
            fp->SetStartDistance(data.startDistance);
        }
    }

    void HeightFogComponent::OnDetachFromWorld()
    {
        if (fp != nullptr) {
            fp->SetFogDensity(0.f);
        }
        fp = nullptr;
    }

    void HeightFogComponent::SetFogColor(const ColorRGB &color)
    {
        data.fogColor = color;
        if (fp != nullptr) {
            fp->SetFogColor(Vector4(color.r, color.g, color.b, 0.f));
        }
    }

    void HeightFogComponent::SetInscatterColor(const ColorRGB &color)
    {
        data.inscatterColor = color;
        if (fp != nullptr) {
            fp->SetInscatterColor(Vector4(color.r, color.g, color.b, 0.f));
        }
    }

    void HeightFogComponent::SetFogDensity(float density)
    {
        data.fogDensity = density;
        if (fp != nullptr) {
            fp->SetFogDensity(density);
        }
    }

    void HeightFogComponent::SetHeightFalloff(float falloff)
    {
        data.heightFalloff = falloff;
        if (fp != nullptr) {
            fp->SetHeightFalloff(falloff);
        }
    }

    void HeightFogComponent::SetBaseHeight(float height)
    {
        data.baseHeight = height;
        if (fp != nullptr) {
            fp->SetBaseHeight(height);
        }
    }

    void HeightFogComponent::SetMaxHeight(float height)
    {
        data.maxHeight = height;
        if (fp != nullptr) {
            fp->SetMaxHeight(height);
        }
    }

    void HeightFogComponent::SetStartDistance(float distance)
    {
        data.startDistance = distance;
        if (fp != nullptr) {
            fp->SetStartDistance(distance);
        }
    }

} // namespace sky
