//
// Shared parsing of material-source JSON (`.mat` / `.mati`): property values
// (type inference + explicit {type,value}) and technique render state.
//

#pragma once

#include <aurora/adaptor/assets/MaterialAsset.h>
#include <core/util/Uuid.h>

#include <framework/serialization/JsonArchive.h>

#include <string>
#include <vector>

namespace sky::aurora::cook {

    // One property parsed from a source "properties" object. `texture` is valid
    // when the property is a texture (the source stores the texture asset Uuid).
    struct ParsedProperty {
        std::string          name;
        MaterialPropertyType type = MaterialPropertyType::FLOAT;
        std::vector<uint8_t> value;
        Uuid                 texture;
    };

    // Parse a "properties" object into typed records (shared by material / instance).
    void ParseMaterialProperties(const rapidjson::Value &properties, std::vector<ParsedProperty> &out);

    // Parse a "techniques" array into MaterialTechniqueData (shader stored as ShaderRef).
    void ParseMaterialTechniques(const rapidjson::Value &techniques, std::vector<MaterialTechniqueData> &out);

    // Parse a "renderState" object; every key is optional (defaults otherwise).
    void ParseMaterialRenderState(const rapidjson::Value &renderState, MaterialRenderState &out);

} // namespace sky::aurora::cook
