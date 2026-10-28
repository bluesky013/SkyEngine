sky_3rd_static(jolt LIBS Jolt)
target_compile_definitions(3rdParty::jolt INTERFACE JPH_DEBUG_RENDERER)
if (APPLE)
    # Keep static SDK globals and header template instantiations in the same native image.
    target_link_options(3rdParty::jolt INTERFACE "LINKER:-unexported_symbol,*3JPH*")
endif ()
