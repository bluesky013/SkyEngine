file(GLOB_RECURSE ADAPTOR_SOURCES src/*.cpp src/*.h include/*.h)
sky_add_module(TARGET JoltPhysicsModule
    SOURCES ${ADAPTOR_SOURCES} JoltModule.cpp
    PRIVATE_INC src/backend
    PUBLIC_INC include
    LINK_LIBS Physics Framework
)
set_property(TARGET JoltPhysicsModule APPEND PROPERTY LINK_LIBRARIES 3rdParty::jolt)
set_property(TARGET JoltPhysicsModule APPEND PROPERTY INTERFACE_LINK_LIBRARIES $<LINK_ONLY:3rdParty::jolt>)

add_custom_command(TARGET JoltPhysicsModule POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:JoltPhysicsModule>/licenses"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_LIST_DIR}/../LICENSE.txt" "$<TARGET_FILE_DIR:JoltPhysicsModule>/licenses/Jolt.txt"
)
