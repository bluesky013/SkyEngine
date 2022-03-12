//
// Created by Zach Lee on 2022/3/12.
//


#pragma once

namespace sky {

    struct BuildRequest {
        std::string srcFolder;
        std::string srcFile;
        std::string dstFolder;
        std::string dstFile;
    };

    class IBuilder {
    public:
        IBuilder() = default;
        virtual ~IBuilder() = default;

        virtual bool Build(const BuildRequest&) = 0;

        virtual bool Support(const std::string& ext) = 0;
    };

    class IBuilderRegistry {
    public:
        IBuilderRegistry() = default;
        ~IBuilderRegistry() = default;

        virtual void RegisterBuilder(IBuilder* builder) = 0;
        virtual void UnRegisterBuilder(IBuilder* builder) = 0;
    };

}