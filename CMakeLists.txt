cmake_minimum_required(VERSION 3.9)
project(MyApp C CXX)

add_application(MyApp
    src/myapp_main.c
    resources/MyApp.r
)

add_application(maccode_spike
    src/spike_main.c
    src/netmac.c
    src/TCPRoutines.c
    src/TCPHi.c
    resources/Spike.r
)
# Apple's MacTCP sample code mixes unsigned long / Ptr (safe on 68K)
# netmac.c casts (StreamPtr)gStream for LowTCPStatus — same class of mismatch
set_source_files_properties(src/TCPRoutines.c src/TCPHi.c src/netmac.c PROPERTIES
    COMPILE_FLAGS "-Wno-incompatible-pointer-types -Wno-int-conversion"
)
# GCC 16 defaults to C23 where 'bool' is a keyword; the vendored TCPRoutines.h
# typedefs bool for C < C23, so compile the spike with C11 to stay compatible.
set_target_properties(maccode_spike PROPERTIES C_STANDARD 11)

add_application(MacCode
    CREATOR "MCde"
    resources/MacCode.r
    src/main.c
    src/netmac.c
    src/proto.c
    src/ui.c
    src/wire.c
    src/transcript.c
    src/TCPRoutines.c
    src/TCPHi.c
)
set_target_properties(MacCode PROPERTIES C_STANDARD 11)
set_source_files_properties(src/TCPRoutines.c src/TCPHi.c src/netmac.c PROPERTIES
    COMPILE_FLAGS "-Wno-incompatible-pointer-types -Wno-int-conversion")
