# ENet static transport used by the network-enet backend plugin.
# zpl-c/enet installs a single header (include/enet.h) and a static lib named enet.
if (WIN32)
    sky_3rd_static(enet LIBS enet EXT_LIBS ws2_32 winmm)
else()
    sky_3rd_static(enet LIBS enet)
endif()
