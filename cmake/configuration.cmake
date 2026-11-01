include(${CMAKE_CURRENT_LIST_DIR}/build_configurations.cmake)

# Mobile targets exclude desktop tools and editor integrations.
if (CMAKE_SYSTEM_NAME MATCHES "^(iOS|Android|OHOS)$")
    set(SKY_MOBILE ON)
else ()
    set(SKY_MOBILE OFF)
endif ()

if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set(PLATFORM_EXT_LIBS
        iconv
        "-framework AudioToolbox"
        "-framework Carbon"
        "-framework Cocoa"
        "-framework CoreAudio"
        "-framework CoreHaptics"
        "-framework CoreVideo"
        "-framework ForceFeedback"
        "-framework GameController"
        "-framework IOKit"
        "-framework Metal")
#    set(PLATFORM_BUNDLE MACOSX_BUNDLE)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
    set(PLATFORM_BUNDLE MACOSX_BUNDLE)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    find_library(log-lib log)
    set(PLATFORM_EXT_LIBS  android ${log-lib})
elseif (CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    if (NOT OHOS_ARCH STREQUAL "arm64-v8a" OR NOT OHOS_STL STREQUAL "c++_shared")
        message(FATAL_ERROR "HarmonyOS requires OHOS_ARCH=arm64-v8a and OHOS_STL=c++_shared")
    endif ()
    if (SKY_THIRD_PARTY_PLATFORM AND NOT SKY_THIRD_PARTY_PLATFORM STREQUAL "HarmonyOS")
        message(FATAL_ERROR "HarmonyOS requires its own third-party output; received ${SKY_THIRD_PARTY_PLATFORM}")
    endif ()
    if (EXISTS "${3RD_PATH}/build_metadata.json")
        file(READ "${3RD_PATH}/build_metadata.json" _sky_ohos_metadata)
        string(JSON _sky_ohos_count LENGTH "${_sky_ohos_metadata}")
        if (_sky_ohos_count GREATER 0)
            math(EXPR _sky_ohos_last "${_sky_ohos_count} - 1")
            foreach (_sky_ohos_index RANGE ${_sky_ohos_last})
                string(JSON _sky_ohos_package MEMBER "${_sky_ohos_metadata}" ${_sky_ohos_index})
                string(JSON _sky_ohos_platform GET "${_sky_ohos_metadata}" "${_sky_ohos_package}" platform)
                if (NOT _sky_ohos_platform STREQUAL "HarmonyOS")
                    message(FATAL_ERROR "HarmonyOS dependency ${_sky_ohos_package} was built for ${_sky_ohos_platform}; rebuild with -p HarmonyOS")
                endif ()
            endforeach ()
        endif ()
    endif ()
    if (SKY_BUILD_SANDBOX OR SKY_BUILD_TOOL)
        message(FATAL_ERROR "HarmonyOS builds runtime libraries; build editor and cook tools on the host")
    endif ()
    include(CheckCXXSourceCompiles)
    include(CMakePushCheckState)
    cmake_push_check_state(RESET)
    set(_sky_ohos_ranges_probe "
        #include <algorithm>
        #include <ranges>
        #include <vector>
        int main() {
            std::vector<int> values{3, 1, 2};
            std::ranges::sort(values);
            return *(values | std::views::reverse).begin();
        }")
    check_cxx_source_compiles("${_sky_ohos_ranges_probe}" SKY_OHOS_HAS_RANGES)
    if (NOT SKY_OHOS_HAS_RANGES)
        set(CMAKE_REQUIRED_FLAGS "-fexperimental-library")
        check_cxx_source_compiles("${_sky_ohos_ranges_probe}" SKY_OHOS_HAS_EXPERIMENTAL_RANGES)
        if (NOT SKY_OHOS_HAS_EXPERIMENTAL_RANGES)
            message(FATAL_ERROR "HarmonyOS SDK cannot compile/link the required C++20 ranges support")
        endif ()
        # SDK libc++ 15 gates ranges behind its supported experimental-library option.
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fexperimental-library>)
        add_link_options($<$<LINK_LANGUAGE:CXX>:-fexperimental-library>)
    endif ()
    cmake_pop_check_state()
    unset(_sky_ohos_ranges_probe)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    find_package(Threads REQUIRED)
    set(PLATFORM_EXT_LIBS ${CMAKE_DL_LIBS} Threads::Threads)
    # Static libraries are linked into shared modules; they must be PIC.
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    # Modules are dlopen'd by bare name from <exe>/; resolve them next to the
    # executable (all binaries land in output/bin).
    set(CMAKE_BUILD_RPATH "$ORIGIN")
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(PLATFORM_EXT_LIBS winmm imm32 version setupapi)
    add_compile_definitions(NOMINMAX)

    if(MSVC)
        cmake_host_system_information(RESULT CPU_NUMBER_OF_LOGICAL_CORES QUERY NUMBER_OF_LOGICAL_CORES)
        add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP${CPU_NUMBER_OF_LOGICAL_CORES}>)
        add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
    endif()

endif()

if (SKY_MATH_SIMD)
    add_compile_definitions(SKY_MATH_SIMD=1)
endif ()

if (SKY_DETERMINISTIC_FP)
    add_compile_definitions(SKY_DETERMINISTIC_FP=1)
    # No FP contraction (a*b+c must not become fma) so results are identical
    # across compilers and architectures (x86/ARM, clang/gcc/msvc).
    if (MSVC)
        add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/fp:precise>)
    else ()
        add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:-ffp-contract=off>)
    endif ()
endif ()

if (SKY_DEVELOP)
    add_compile_definitions(SKY_DEVELOP=1)
endif ()

add_compile_definitions("$<$<CONFIG:Debug>:_DEBUG;DEBUG>")
