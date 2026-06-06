cmake_minimum_required(VERSION 3.9)
project(MyApp C CXX)

add_application(MyApp
    src/main.c
    resources/MyApp.r
)

add_application(maccode_spike
    src/spike_main.c
    src/TCPRoutines.c
    src/TCPHi.c
    resources/Spike.r
)
# Apple's MacTCP sample code mixes unsigned long / Ptr (safe on 68K)
set_source_files_properties(src/TCPRoutines.c src/TCPHi.c PROPERTIES
    COMPILE_FLAGS "-Wno-incompatible-pointer-types -Wno-int-conversion"
)
# GCC 16 defaults to C23 where 'bool' is a keyword; the vendored TCPRoutines.h
# typedefs bool for C < C23, so compile the spike with C11 to stay compatible.
set_target_properties(maccode_spike PROPERTIES C_STANDARD 11)
