//
// Created by Zach Lee on 2021/12/16.
//

#pragma once
#include <editor/inspector/PropertyWidget.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/serialization/PropertyCommon.h>

namespace sky::editor::util {

    bool CheckProperty(const PropertyMap& properties, CommonPropertyKey key, bool dft = true);

} // sky::editor::util
