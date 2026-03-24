# cmake/vcpkg.cmake
# Bridge module: creates 3rdParty:: targets from vcpkg-installed packages.
# Included automatically when vcpkg toolchain is detected.

include(${ENGINE_ROOT}/cmake/thirdparty_helpers.cmake)

# ---------------------------------------------------------------------------
# Helper: create a 3rdParty::<name> INTERFACE target forwarding to vcpkg
# targets, but only if the alias does not already exist.
# ---------------------------------------------------------------------------
function(sky_vcpkg_alias ALIAS_NAME)
    if (TARGET ${ALIAS_NAME})
        return()
    endif()
    add_library(${ALIAS_NAME} INTERFACE IMPORTED GLOBAL)
    target_link_libraries(${ALIAS_NAME} INTERFACE ${ARGN})
endfunction()

# ===========================================================================
# Core dependencies (always required)
# ===========================================================================

# --- boost ------------------------------------------------------------------
find_package(Boost REQUIRED COMPONENTS container graph)
sky_vcpkg_alias(3rdParty::boost Boost::container Boost::graph)

# --- sfmt -------------------------------------------------------------------
find_package(sfmt CONFIG REQUIRED)
sky_vcpkg_alias(3rdParty::sfmt sfmt::sfmt)

# --- taskflow ---------------------------------------------------------------
find_package(Taskflow CONFIG REQUIRED)
sky_vcpkg_alias(3rdParty::taskflow Taskflow::Taskflow)

# --- rapidjson --------------------------------------------------------------
find_package(RapidJSON CONFIG REQUIRED)
sky_vcpkg_alias(3rdParty::rapidjson rapidjson)

# --- sdl --------------------------------------------------------------------
if (NOT ANDROID AND NOT IOS)
    find_package(SDL2 CONFIG REQUIRED)
    sky_vcpkg_alias(3rdParty::sdl SDL2::SDL2 SDL2::SDL2main)
endif()

# --- vulkan-memory-allocator ------------------------------------------------
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
sky_vcpkg_alias(3rdParty::vma GPUOpen::VulkanMemoryAllocator)

# --- imgui ------------------------------------------------------------------
find_package(imgui CONFIG REQUIRED)
sky_vcpkg_alias(3rdParty::imgui imgui::imgui)

# --- glslang ----------------------------------------------------------------
find_package(glslang CONFIG REQUIRED)
sky_vcpkg_alias(3rdParty::glslang
    glslang::glslang
    glslang::glslang-default-resource-limits
    glslang::SPIRV
    glslang::SPVRemapper
)

# --- SPIRV-Cross ------------------------------------------------------------
find_package(spirv_cross_core CONFIG REQUIRED)
find_package(spirv_cross_glsl CONFIG REQUIRED)
find_package(spirv_cross_msl  CONFIG REQUIRED)
find_package(spirv_cross_cpp  CONFIG REQUIRED)
find_package(spirv_cross_reflect CONFIG REQUIRED)
find_package(spirv_cross_util CONFIG REQUIRED)
sky_vcpkg_alias(3rdParty::SPIRVCross
    spirv-cross-glsl
    spirv-cross-msl
    spirv-cross-cpp
    spirv-cross-core
    spirv-cross-reflect
    spirv-cross-util
)

# --- dxcompiler (Windows only) ----------------------------------------------
if (WIN32)
    find_package(directx-dxc CONFIG REQUIRED)
    sky_vcpkg_alias(3rdParty::dxcompiler Microsoft::DirectXShaderCompiler)
endif()

# --- googletest -------------------------------------------------------------
find_package(GTest CONFIG REQUIRED)
sky_vcpkg_alias(3rdParty::googletest GTest::gtest)

# ===========================================================================
# Optional dependencies (controlled by CMake options)
# ===========================================================================

# --- cpython ----------------------------------------------------------------
if (SKY_BUILD_CPYTHON)
    find_package(Python3 COMPONENTS Development REQUIRED)
    sky_vcpkg_alias(3rdParty::cpython Python3::Python)
endif()

# --- lz4 (compression plugin) ----------------------------------------------
if (SKY_BUILD_COMPRESSION)
    find_package(lz4 CONFIG REQUIRED)
    sky_vcpkg_alias(3rdParty::lz4 lz4::lz4)
endif()

# --- freetype (text plugin) -------------------------------------------------
if (SKY_BUILD_FREETYPE)
    find_package(Freetype REQUIRED)
    sky_vcpkg_alias(3rdParty::freetype Freetype::Freetype)
endif()

# --- tracy (profiler) -------------------------------------------------------
if (SKY_USE_TRACY)
    find_package(Tracy CONFIG REQUIRED)
    sky_vcpkg_alias(3rdParty::tracy Tracy::TracyClient)
    add_definitions(-DTRACY_ENABLE)
endif()

# --- bullet3 (physics plugin) ----------------------------------------------
if (SKY_BUILD_BULLET)
    find_package(Bullet CONFIG REQUIRED)
    sky_vcpkg_alias(3rdParty::bullet3
        Bullet3Common::Bullet3Common
        Bullet3Collision::Bullet3Collision
        Bullet3Dynamics::Bullet3Dynamics
        Bullet3Geometry::Bullet3Geometry
        BulletCollision::BulletCollision
        BulletDynamics::BulletDynamics
        LinearMath::LinearMath
    )
endif()

# --- recast (navigation plugin) --------------------------------------------
if (SKY_BUILD_RECAST)
    find_package(recastnavigation CONFIG REQUIRED)
    sky_vcpkg_alias(3rdParty::recast
        RecastNavigation::DebugUtils
        RecastNavigation::Detour
        RecastNavigation::DetourCrowd
        RecastNavigation::DetourTileCache
        RecastNavigation::Recast
    )
endif()

# ===========================================================================
# Editor dependencies (SKY_BUILD_EDITOR)
# ===========================================================================
if (SKY_BUILD_EDITOR)
    # --- assimp -------------------------------------------------------------
    find_package(assimp CONFIG REQUIRED)
    sky_vcpkg_alias(3rdParty::assimp assimp::assimp)

    # --- meshoptimizer ------------------------------------------------------
    find_package(meshoptimizer CONFIG REQUIRED)
    sky_vcpkg_alias(3rdParty::meshoptimizer meshoptimizer::meshoptimizer)

    # --- stb ----------------------------------------------------------------
    find_package(Stb REQUIRED)
    if (NOT TARGET 3rdParty::stb)
        add_library(3rdParty::stb INTERFACE IMPORTED GLOBAL)
        target_include_directories(3rdParty::stb INTERFACE ${Stb_INCLUDE_DIR})
    endif()

    # --- ImGuizmo -----------------------------------------------------------
    find_package(imguizmo CONFIG REQUIRED)
    sky_vcpkg_alias(3rdParty::ImGuizmo imguizmo::imguizmo)

    # --- GKlib (custom registry) --------------------------------------------
    find_package(gklib CONFIG REQUIRED)
    sky_vcpkg_alias(3rdParty::GKlib gklib::gklib)

    # --- metis --------------------------------------------------------------
    find_package(metis CONFIG REQUIRED)
    sky_vcpkg_alias(3rdParty::metis metis::metis)

    # --- ispc_texcomp (custom registry) -------------------------------------
    find_package(ispc_texcomp CONFIG REQUIRED)
    sky_vcpkg_alias(3rdParty::ispc_texcomp ispc_texcomp::ispc_texcomp)
endif()

message(STATUS "[SkyEngine] Third-party libraries resolved via vcpkg")
