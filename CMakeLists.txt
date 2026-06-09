cmake_minimum_required(VERSION 3.9)
project(MacCode VERSION 1.1.0 LANGUAGES C)

add_application(MacCode
    CREATOR "MCde"
    resources/MacCode.r
    src/main.c
    src/netmac.c
    src/proto.c
    src/prefs.c
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

# After MacCode_APPL packages the app: (1) Retro68's Rez doesn't set the Finder "bundle"
# bit, so patch it (and fix the MacBinary CRC) so the Finder uses our BNDL/icon; then
# (2) emit a versioned copy (MacCode-v<version>.bin) as the distributable artifact.
add_custom_command(TARGET MacCode_APPL POST_BUILD
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/set_bundle_bit.py ${CMAKE_CURRENT_BINARY_DIR}/MacCode.bin
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/MacCode.bin
            ${CMAKE_CURRENT_BINARY_DIR}/MacCode-v${PROJECT_VERSION}.bin
    COMMENT "Finder bundle bit + versioned MacCode-v${PROJECT_VERSION}.bin")
