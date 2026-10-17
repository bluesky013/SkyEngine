add_library(Aurora.FrameBackend.Static STATIC FrameContext.cpp FrameContext.h FrameTransientPool.cpp FrameTransientPool.h)
target_include_directories(Aurora.FrameBackend.Static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(Aurora.FrameBackend.Static PUBLIC Aurora.RHI.Interface)
set_target_properties(Aurora.FrameBackend.Static PROPERTIES FOLDER "${CURRENT_FOLDER}" POSITION_INDEPENDENT_CODE ON)
