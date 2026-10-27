file(GLOB_RECURSE ADAPTOR_SOURCES src/*.cpp src/*.h include/*.h)
sky_add_library(TARGET JoltPhysics.Adaptor.Static STATIC
    SOURCES ${ADAPTOR_SOURCES}
    PRIVATE_INC src/backend src/assets ../shared
    PUBLIC_INC include
    LINK_LIBS Physics Framework JoltPhysics.AssetCodec.Static
)
set_target_properties(JoltPhysics.Adaptor.Static PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_property(TARGET JoltPhysics.Adaptor.Static APPEND PROPERTY LINK_LIBRARIES 3rdParty::jolt)
set_property(TARGET JoltPhysics.Adaptor.Static APPEND PROPERTY INTERFACE_LINK_LIBRARIES $<LINK_ONLY:3rdParty::jolt>)

sky_add_module(TARGET JoltPhysicsModule
    SOURCES JoltModule.cpp
    PRIVATE_INC src/backend src/assets ../shared
    LINK_LIBS JoltPhysics.Adaptor.Static
)

add_custom_command(TARGET JoltPhysicsModule POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:JoltPhysicsModule>/licenses"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_LIST_DIR}/../LICENSE.txt" "$<TARGET_FILE_DIR:JoltPhysicsModule>/licenses/Jolt.txt"
)
