//
// Created by Zach Lee on 2022/3/13.
//


#include <framework/application/SettingRegistry.h>
#include <rapidjson/prettywriter.h>

using namespace rapidjson;

namespace sky {

    void SettingRegistry::Swap(SettingRegistry& registry)
    {
        document.Swap(registry.document);
    }

    void SettingRegistry::Save(std::string& out) const
    {
        StringBuffer buffer;
        PrettyWriter<StringBuffer> writer(buffer);
        document.Accept(writer);
        out = buffer.GetString();
    }

}