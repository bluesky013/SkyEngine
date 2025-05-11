set(LIB_NAME "imgui")
set(TARGET_WITH_NAMESPACE "3rdParty::${LIB_NAME}")
if (TARGET ${TARGET_WITH_NAMESPACE})
    return()
endif()

string(REPLACE "\\" "/" NORM_PATH "${${LIB_NAME}_PATH}")  # 转换为正斜杠
file(GLOB_RECURSE SRC_FILES ${NORM_PATH}/src/*)

add_library(${LIB_NAME} STATIC ${SRC_FILES})
target_include_directories(${LIB_NAME} PUBLIC ${NORM_PATH}/src)

add_library(${TARGET_WITH_NAMESPACE} ALIAS ${LIB_NAME})
set(${LIB_NAME}_FOUND True)