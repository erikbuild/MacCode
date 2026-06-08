cmake_minimum_required(VERSION 3.9)
project(MacCode C)

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

# GCC defaults to C23 where 'bool' is a keyword; the vendored TCPRoutines.h
# typedefs bool for C < C23, so build with C11 to stay compatible.
set_target_properties(MacCode PROPERTIES C_STANDARD 11)

# Apple's MacTCP sample code mixes unsigned long / Ptr (safe on 68K); netmac.c
# casts (StreamPtr)gStream for LowTCPStatus — the same class of mismatch.
set_source_files_properties(src/TCPRoutines.c src/TCPHi.c src/netmac.c PROPERTIES
    COMPILE_FLAGS "-Wno-incompatible-pointer-types -Wno-int-conversion")
