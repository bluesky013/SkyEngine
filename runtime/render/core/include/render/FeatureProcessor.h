//
// Created by Zach Lee on 2023/9/7.
//

#pragma once

#include <cstdint>
#include <core/type/Rtti.h>

namespace sky {
    namespace rdg {
        struct RenderGraph;
    } // namespace rdg

    class RenderScene;

    class IFeatureProcessor {
    public:
        explicit IFeatureProcessor(RenderScene *scn) : scene(scn) {}
        virtual ~IFeatureProcessor() = default;

        virtual void Tick(float time) = 0;
        virtual void Render(rdg::RenderGraph &rdg) = 0;

        uint32_t GetTypeID() const { return typeID; }

    protected:
        friend class IFeatureProcessorBuilder;
        uint32_t typeID = 0;
        RenderScene *scene = nullptr;
    };

    class IFeatureProcessorBuilder {
    public:
        IFeatureProcessorBuilder() = default;
        virtual ~IFeatureProcessorBuilder() = default;

        virtual IFeatureProcessor *BuildFeatureProcessor(RenderScene *scene) = 0;
    protected:
        static void SetFeatureID(IFeatureProcessor *feature, uint32_t id)
        {
            feature->typeID = id;
        }
    };

    template <typename T>
    class FeatureProcessorBuilder : public IFeatureProcessorBuilder {
    public:
        FeatureProcessorBuilder() = default;
        ~FeatureProcessorBuilder() override = default;

        IFeatureProcessor *BuildFeatureProcessor(RenderScene *scene) override
        {
            auto *feature = new T(scene);
            IFeatureProcessorBuilder::SetFeatureID(feature, RuntimeTypeId<T>());
            return feature;
        }
    };

} // namespace sky
