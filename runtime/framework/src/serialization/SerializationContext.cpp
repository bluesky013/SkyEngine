//
// Created by Zach Lee on 2021/12/9.
//

#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/CoreReflection.h>
#include <framework/serialization/ArrayVisitor.h>
namespace sky {

    void JsonLoad(std::string &str, JsonInputArchive &archive)
    {
        str = archive.LoadString();
    }

    void JsonStore(const std::string &str, JsonOutputArchive &archive)
    {
        archive.SaveValue(str);
    }

    SerializationContext::SerializationContext()
    {
        Register<uint64_t>("uint64_t");
        Register<uint32_t>("uint32_t");
        Register<uint16_t>("uint16_t");
        Register<uint8_t>("uint8_t");
        Register<int64_t>("int64_t");
        Register<int32_t>("int32_t");
        Register<int16_t>("int16_t");
        Register<int8_t>("int8_t");
        Register<bool>("bool");
        Register<float>("float");
        Register<double>("double");
        Register<SequenceVisitor>("SequenceVisitor");

        Register<std::string>("String")
                .JsonLoad<&JsonLoad>()
                .JsonSave<&JsonStore>();

        CoreReflection(this);
    }

    TypeNode *SerializationContext::FindType(const std::string &key)
    {
        auto iter = lookupTable.find(key);
        return iter == lookupTable.end() ? nullptr : iter->second;
    }

    TypeNode *SerializationContext::FindTypeById(const Uuid &id)
    {
        auto iter = types.find(id);
        return iter == types.end() ? nullptr : &iter->second;
    }

} // namespace sky
