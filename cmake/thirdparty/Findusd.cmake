set(LIB_NAME "usd")
set(TARGET_WITH_NAMESPACE "3rdParty::${LIB_NAME}")
if (TARGET ${TARGET_WITH_NAMESPACE})
    return()
endif ()

# OpenUSD minimal monolithic 构建（无 Python/Hydra/Imaging，仅核心 + usdGeom/usdShade/usdSkel）
# 流水线对 Debug/Release 各产出一份（lib/Debug, lib/Release），按 CONFIG 分别链接
set(${LIB_NAME}_INCLUDE_DIR ${${LIB_NAME}_PATH}/include)
set(${LIB_NAME}_LIBS_DIR ${${LIB_NAME}_PATH}/lib)

if (APPLE)
    set(${LIB_NAME}_LIBRARY_DEBUG   ${${LIB_NAME}_LIBS_DIR}/Debug/libusd_ms.dylib)
    set(${LIB_NAME}_LIBRARY_RELEASE ${${LIB_NAME}_LIBS_DIR}/Release/libusd_ms.dylib)
elseif (WIN32)
    set(${LIB_NAME}_LIBRARY_DEBUG   ${${LIB_NAME}_LIBS_DIR}/Debug/usd_ms.lib)
    set(${LIB_NAME}_LIBRARY_RELEASE ${${LIB_NAME}_LIBS_DIR}/Release/usd_ms.lib)
else ()
    set(${LIB_NAME}_LIBRARY_DEBUG   ${${LIB_NAME}_LIBS_DIR}/Debug/libusd_ms.so)
    set(${LIB_NAME}_LIBRARY_RELEASE ${${LIB_NAME}_LIBS_DIR}/Release/libusd_ms.so)
endif ()

set(${LIB_NAME}_LIBRARY
    "$<$<CONFIG:debug>:${${LIB_NAME}_LIBRARY_DEBUG}>"
    "$<$<CONFIG:release>:${${LIB_NAME}_LIBRARY_RELEASE}>")

# 动态库运行时拷贝（functions.cmake 的 INTERFACE_DYN_LIBS 机制支持 CONFIG 生成器表达式）；
# 注意必须拼成单个生成器表达式（两种配置各一段），保证任意配置下求值恰好为一个路径
if (WIN32)
    # Windows 下流水线 bin/ 不按配置分目录，固定用 Release
    set(${LIB_NAME}_DYNAMIC_LIBRARY ${${LIB_NAME}_PATH}/bin/usd_ms.dll)
else ()
    set(${LIB_NAME}_DYNAMIC_LIBRARY
        "$<$<CONFIG:debug>:${${LIB_NAME}_LIBRARY_DEBUG}>$<$<CONFIG:release>:${${LIB_NAME}_LIBRARY_RELEASE}>")
endif ()

add_library(${TARGET_WITH_NAMESPACE} INTERFACE IMPORTED GLOBAL)
target_include_directories(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_INCLUDE_DIR})
target_link_libraries(${TARGET_WITH_NAMESPACE} INTERFACE ${${LIB_NAME}_LIBRARY})

# USD 的 sdf/work 等头文件直接包含 <tbb/...>，libusd_ms 也动态链接 tbb。
# 优先使用 thirdparty 流水线产出的 oneTBB（thirdparty.json 的 tbb 包，Debug 产物名为 tbb_debug），
# 本地手工构建可能链接的是 homebrew tbb（仅 Release），作为回退。
set(USD_TBB_3RD_DIR ${${LIB_NAME}_PATH}/../tbb)
find_path(USD_TBB_INCLUDE_DIR tbb/concurrent_queue.h
    HINTS ${USD_TBB_3RD_DIR}/include /opt/homebrew/opt/tbb/include)
find_library(USD_TBB_LIBRARY_RELEASE NAMES tbb
    HINTS ${USD_TBB_3RD_DIR}/lib/Release /opt/homebrew/opt/tbb/lib)
find_library(USD_TBB_LIBRARY_DEBUG NAMES tbb_debug
    HINTS ${USD_TBB_3RD_DIR}/lib/Debug)

if (USD_TBB_INCLUDE_DIR AND USD_TBB_LIBRARY_RELEASE)
    if (NOT USD_TBB_LIBRARY_DEBUG)
        set(USD_TBB_LIBRARY_DEBUG ${USD_TBB_LIBRARY_RELEASE})
    endif ()
    set(USD_TBB_LIBRARY
        "$<$<CONFIG:debug>:${USD_TBB_LIBRARY_DEBUG}>"
        "$<$<CONFIG:release>:${USD_TBB_LIBRARY_RELEASE}>")
    target_include_directories(${TARGET_WITH_NAMESPACE} INTERFACE ${USD_TBB_INCLUDE_DIR})
    target_link_libraries(${TARGET_WITH_NAMESPACE} INTERFACE ${USD_TBB_LIBRARY})
    set(${LIB_NAME}_DYNAMIC_LIBRARY
        ${${LIB_NAME}_DYNAMIC_LIBRARY}
        "$<$<CONFIG:debug>:${USD_TBB_LIBRARY_DEBUG}>$<$<CONFIG:release>:${USD_TBB_LIBRARY_RELEASE}>")
else ()
    message(WARNING "tbb not found, usd headers may fail to compile")
endif ()

set_target_properties(${TARGET_WITH_NAMESPACE} PROPERTIES INTERFACE_DYN_LIBS "${${LIB_NAME}_DYNAMIC_LIBRARY}")

set(${LIB_NAME}_FOUND True)
