file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

add_library(Aurora.RHI.Interface INTERFACE)
target_compile_features(Aurora.RHI.Interface INTERFACE cxx_std_20)
target_include_directories(Aurora.RHI.Interface INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(Aurora.RHI.Interface INTERFACE Core)
install(TARGETS Aurora.RHI.Interface EXPORT SkyEngineExport)

sky_add_library(TARGET Aurora.RHI.Static STATIC
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Aurora.RHI.Interface
    INSTALL_DIR
        include
)
