vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO bluesky013/ISPCTextureCompressor
  REF "${VERSION}"
  SHA512 0
  HEAD_REF master
)

vcpkg_cmake_configure(
  SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_cmake_install()
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${SOURCE_PATH}/ispc_texcomp/license.txt"
  DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
