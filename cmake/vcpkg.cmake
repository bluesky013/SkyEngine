# cmake/vcpkg.cmake
# Bridge module: creates 3rdParty:: targets from vcpkg-installed packages.
# Included automatically when vcpkg toolchain is detected.

Include(${ENGINE_ROOT}/cmake/thirdparty_helpers.cmake)

# CMake 4.x strict validation of transitive INTERFACE_LINK_LIBRARIES can fail
# when targets are created via function overrides (like vcpkg's add_library).
# Disable CMP0189 to use pre-4.1 behavior.
if(POLICY CMP0189)
    cmake_policy(SET CMP0189 OLD)
endif()

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
# Pre-create Boost::headers as GLOBAL with explicit include path to work around
# CMake 4.x issue where _IMPORT_PREFIX computes incorrectly in vcpkg's function-override
# context, resulting in "${_IMPORT_PREFIX}/include" = "/include" at generate time.
if(NOT TARGET Boost::headers)
    find_path(_SKY_BOOST_INC boost/version.hpp
        PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include"
        NO_DEFAULT_PATH)
    if(_SKY_BOOST_INC)
        _add_library(Boost::headers INTERFACE IMPORTED GLOBAL)
        set_target_properties(Boost::headers PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${_SKY_BOOST_INC}")
    endif()
endif()

find_package(Boost REQUIRED COMPONENTS container graph)

# Create/replace 3rdParty::boost as a proper INTERFACE IMPORTED target that directly uses 
# the found boost include/lib paths WITHOUT relying on Boost::container/Boost::graph targets,
# which may not be visible due to CMake 4.x scoping issues in vcpkg's function overrides.

# Find boost include directory directly
find_path(_SKY_BOOST_INC boost/version.hpp
    PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include"
    NO_DEFAULT_PATH)

# Find boost libraries directly  
find_library(_SKY_BOOST_GRAPH libboost_graph.a
         PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib"
         NO_DEFAULT_PATH)
find_library(_SKY_BOOST_CONTAINER libboost_container.a
    PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib"
    NO_DEFAULT_PATH)

# Create or replace 3rdParty::boost
if(TARGET 3rdParty::boost)
    # Target already exists (possibly created implicitly), replace its properties
    set_target_properties(3rdParty::boost PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${_SKY_BOOST_INC}"
        INTERFACE_LINK_LIBRARIES "")
    target_link_libraries(3rdParty::boost INTERFACE ${_SKY_BOOST_CONTAINER} ${_SKY_BOOST_GRAPH})
else()
    # Create new target
    add_library(3rdParty::boost INTERFACE IMPORTED GLOBAL)
    set_target_properties(3rdParty::boost PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${_SKY_BOOST_INC}")
    if(_SKY_BOOST_CONTAINER OR _SKY_BOOST_GRAPH)
        target_link_libraries(3rdParty::boost INTERFACE ${_SKY_BOOST_CONTAINER} ${_SKY_BOOST_GRAPH})
    endif()
endif()

unset(_SKY_BOOST_INC)
unset(_SKY_BOOST_CONTAINER)
unset(_SKY_BOOST_GRAPH)

# --- sfmt -------------------------------------------------------------------
find_package(sfmt CONFIG REQUIRED)
sky_vcpkg_alias(3rdParty::sfmt sfmt::SFMT)

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
    # Pre-create ZLIB::ZLIB as GLOBAL so freetype-targets.cmake (CMake 4.x)
    # can validate it in INTERFACE_LINK_LIBRARIES at generate time.
    if(NOT TARGET ZLIB::ZLIB)
        find_path(_sky_zlib_inc NAMES zlib.h
            PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include"
            NO_DEFAULT_PATH)
        find_library(_sky_zlib_rel NAMES z zlib
            PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib"
            NO_DEFAULT_PATH)
        find_library(_sky_zlib_dbg NAMES z zlib
            PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib"
            NO_DEFAULT_PATH)
        _add_library(ZLIB::ZLIB UNKNOWN IMPORTED GLOBAL)
        set_target_properties(ZLIB::ZLIB PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${_sky_zlib_inc}"
            IMPORTED_LOCATION "${_sky_zlib_rel}")
        if(_sky_zlib_rel)
            set_property(TARGET ZLIB::ZLIB APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
            set_target_properties(ZLIB::ZLIB PROPERTIES IMPORTED_LOCATION_RELEASE "${_sky_zlib_rel}")
        endif()
        if(_sky_zlib_dbg)
            set_property(TARGET ZLIB::ZLIB APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
            set_target_properties(ZLIB::ZLIB PROPERTIES IMPORTED_LOCATION_DEBUG "${_sky_zlib_dbg}")
        endif()
        set(ZLIB_FOUND TRUE CACHE BOOL "" FORCE)
        set(ZLIB_LIBRARY "${_sky_zlib_rel}" CACHE FILEPATH "" FORCE)
        set(ZLIB_INCLUDE_DIRS "${_sky_zlib_inc}" CACHE PATH "" FORCE)
    endif()
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
        Bullet3Common
        Bullet3Collision
        Bullet3Dynamics
        Bullet3Geometry
        BulletCollision
        BulletDynamics
        LinearMath
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
