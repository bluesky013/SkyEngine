# Deploy the editor's builtin resources next to the executable.
#
# A symbolic link avoids duplicating a large resource tree while still tracking
# source edits without a rebuild. When symlinks are unavailable (e.g. Windows
# without developer mode) the deployment falls back to a recursive copy.
#
# Usage: cmake -DSRC_DIR=<dir> -DDST_DIR=<dir> -P deploy_resources.cmake

if (NOT IS_DIRECTORY "${SRC_DIR}")
    message(FATAL_ERROR "editor resource source dir not found: ${SRC_DIR}")
endif ()

# Drop any previous deployment (stale symlink or copied tree) first. Remove a
# symlink itself, never the directory it points at.
if (IS_SYMLINK "${DST_DIR}")
    file(REMOVE "${DST_DIR}")
elseif (IS_DIRECTORY "${DST_DIR}")
    file(REMOVE_RECURSE "${DST_DIR}")
elseif (EXISTS "${DST_DIR}")
    file(REMOVE "${DST_DIR}")
endif ()

get_filename_component(_dst_parent "${DST_DIR}" DIRECTORY)
file(MAKE_DIRECTORY "${_dst_parent}")

execute_process(
    COMMAND ${CMAKE_COMMAND} -E create_symlink "${SRC_DIR}" "${DST_DIR}"
    RESULT_VARIABLE _link_result
    OUTPUT_QUIET ERROR_QUIET)

if (_link_result EQUAL 0)
    message(STATUS "Deployed editor resources (symlink): ${DST_DIR}")
else ()
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${SRC_DIR}" "${DST_DIR}"
        RESULT_VARIABLE _copy_result
        OUTPUT_QUIET ERROR_QUIET)
    if (NOT _copy_result EQUAL 0)
        message(FATAL_ERROR "failed to deploy editor resources to: ${DST_DIR}")
    endif ()
    message(STATUS "Deployed editor resources (copied; symlink unavailable): ${DST_DIR}")
endif ()

unset(_dst_parent)
unset(_link_result)
unset(_copy_result)
