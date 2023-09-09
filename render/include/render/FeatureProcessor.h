//
// Created by Zach Lee on 2023/9/7.
//

#pragma once

#include <cstdint>
#include <core/type/Type.h>

namespace sky {

    class IFeatureProcessor {
    public:
        IFeatureProcessor() = default;
        virtual ~IFeatureProcessor() = default;

        virtual void Tick(float time) = 0;
        virtual void Render(float time) = 0;

        uint32_t GetTypeID() const { return typeID; }

    protected:
        friend class IFeatureProcessorBuilder;
        uint32_t typeID = 0;
    };

    class IFeatureProcessorBuilder {
    public:
        IFeatureProcessorBuilder() = default;
        virtual ~IFeatureProcessorBuilder() = default;

        virtual IFeatureProcessor *BuildFeatureProcessor() = 0;
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
        ~FeatureProcessorBuilder() = default;

        IFeatureProcessor *BuildFeatureProcessor() override
        {
            auto *feature = new T();
            IFeatureProcessorBuilder::SetFeatureID(feature, TypeInfo<T>::Hash());
            return feature;
        }
    };

} // namespace sky
