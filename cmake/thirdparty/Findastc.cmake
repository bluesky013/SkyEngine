# astc-encoder (ARM) codec. The package builds the default "native" ISA
# artifact (see astc-encoder Source/CMakeLists.txt), so the installed static
# library is astcenc-native-static. cmake/patches/astc.patch adds the install
# rules that the upstream CMake only provides when ASTCENC_CLI is enabled.
sky_3rd_static(astc LIBS astcenc-native-static)
