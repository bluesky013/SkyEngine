file(GLOB_RECURSE SRC_FILES src/*)
file(GLOB_RECURSE INC_FILES include/*)

unset(AURORA_VULKAN_ICD_MANIFEST)
unset(AURORA_VULKAN_LAYER_MANIFEST)

if (WIN32)
    file(GLOB_RECURSE PLATFORM_SRC platform/windows/*)
elseif(APPLE)
    file(GLOB_RECURSE PLATFORM_SRC platform/macos/*)
elseif(ANDROID)
    file(GLOB_RECURSE PLATFORM_SRC platform/android/*)
endif()

add_library(MoltenVk INTERFACE IMPORTED GLOBAL)
if (APPLE)
    find_package(Vulkan REQUIRED)

    set(MoltenVk_DYN_LIBS "")
    foreach(vulkan_lib ${Vulkan_LIBRARIES})
        if (EXISTS "${vulkan_lib}")
            list(APPEND MoltenVk_DYN_LIBS "${vulkan_lib}")
            get_filename_component(MoltenVk_LIB_DIR "${vulkan_lib}" DIRECTORY)
        endif()
    endforeach()

    if (MoltenVk_LIB_DIR)
        get_filename_component(MoltenVk_SDK_ROOT "${MoltenVk_LIB_DIR}" DIRECTORY)
        set(MoltenVk_SHARE_DIR "${MoltenVk_SDK_ROOT}/share/vulkan")
        set(AURORA_VULKAN_MANIFEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/vulkan")
        set(AURORA_VULKAN_ICD_MANIFEST "${AURORA_VULKAN_MANIFEST_DIR}/icd.d/MoltenVK_icd.json")
        set(AURORA_VULKAN_LAYER_MANIFEST "${AURORA_VULKAN_MANIFEST_DIR}/explicit_layer.d/VkLayer_khronos_validation.json")

        file(MAKE_DIRECTORY "${AURORA_VULKAN_MANIFEST_DIR}/icd.d")
        file(MAKE_DIRECTORY "${AURORA_VULKAN_MANIFEST_DIR}/explicit_layer.d")

        if (EXISTS "${MoltenVk_SHARE_DIR}/icd.d/MoltenVK_icd.json")
            file(READ "${MoltenVk_SHARE_DIR}/icd.d/MoltenVK_icd.json" MoltenVk_ICD_CONTENT)
            string(REPLACE "../../../lib/libMoltenVK.dylib" "../../libMoltenVK.dylib" MoltenVk_ICD_CONTENT "${MoltenVk_ICD_CONTENT}")
            file(WRITE "${AURORA_VULKAN_ICD_MANIFEST}" "${MoltenVk_ICD_CONTENT}")
        else()
            unset(AURORA_VULKAN_ICD_MANIFEST)
        endif()

        if (EXISTS "${MoltenVk_SHARE_DIR}/explicit_layer.d/VkLayer_khronos_validation.json")
            file(READ "${MoltenVk_SHARE_DIR}/explicit_layer.d/VkLayer_khronos_validation.json" MoltenVk_LAYER_CONTENT)
            string(REPLACE
                "../../../lib/libVkLayer_khronos_validation.dylib"
                "../../libVkLayer_khronos_validation.dylib"
                MoltenVk_LAYER_CONTENT
                "${MoltenVk_LAYER_CONTENT}")
            file(WRITE "${AURORA_VULKAN_LAYER_MANIFEST}" "${MoltenVk_LAYER_CONTENT}")
        else()
            unset(AURORA_VULKAN_LAYER_MANIFEST)
        endif()

        foreach(moltenvk_lib
            libMoltenVK.dylib
            libvulkan.1.dylib
            libvulkan_kosmickrisp.dylib
            libVkLayer_screenshot.dylib
            libVkLayer_khronos_validation.dylib
            libVkLayer_khronos_synchronization2.dylib
            libVkLayer_khronos_shader_object.dylib
            libVkLayer_khronos_profiles.dylib
            libVkLayer_gfxreconstruct.dylib
            libVkLayer_device_simulation.dylib
        )
            if (EXISTS "${MoltenVk_LIB_DIR}/${moltenvk_lib}")
                list(APPEND MoltenVk_DYN_LIBS "${MoltenVk_LIB_DIR}/${moltenvk_lib}")
            endif()
        endforeach()
    endif()

    list(REMOVE_DUPLICATES MoltenVk_DYN_LIBS)
    if (MoltenVk_DYN_LIBS)
        set_property(TARGET MoltenVk PROPERTY INTERFACE_DYN_LIBS "${MoltenVk_DYN_LIBS}")
    endif()
endif()

sky_add_library(TARGET AuroraVulkan SHARED
    SOURCES
        ${SRC_FILES}
        ${INC_FILES}
        ${PLATFORM_SRC}
    PRIVATE_INC
        src
    PUBLIC_INC
        include
    LINK_LIBS
        Core
        Aurora.RHI
        3rdParty::vulkan_headers
        3rdParty::vma
        MoltenVk
)

if (APPLE)
    if (AURORA_VULKAN_ICD_MANIFEST)
        add_custom_command(TARGET AuroraVulkan POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/vulkan/icd.d
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${AURORA_VULKAN_ICD_MANIFEST}
                    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/vulkan/icd.d/MoltenVK_icd.json
        )
    endif()

    if (AURORA_VULKAN_LAYER_MANIFEST)
        add_custom_command(TARGET AuroraVulkan POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/vulkan/explicit_layer.d
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${AURORA_VULKAN_LAYER_MANIFEST}
                    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/vulkan/explicit_layer.d/VkLayer_khronos_validation.json
        )
    endif()
endif()

target_compile_definitions(AuroraVulkan PUBLIC VK_NO_PROTOTYPES)

#if (SKY_BUILD_XR)
#    target_compile_definitions(AuroraVulkan PUBLIC -DXR_USE_GRAPHICS_API_VULKAN)
#endif ()

if(ANDROID)
    target_compile_definitions(AuroraVulkan PUBLIC -DVMA_VULKAN_VERSION=1002000)
endif()

sky_add_dependency(TARGET AuroraVulkan DEPENDENCIES Launcher Editor AuroraTest)
