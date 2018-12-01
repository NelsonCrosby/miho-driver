cmake_minimum_required(VERSION 2.6)

project(MihoDriver)

set(lib_src src/tempmain.c)
set(lib_link)

### MAIN MIHO SOURCES ###
set(lib_src ${lib_src}
    src/exceptions.c
)

### VENDORED DEPENDENCIES ###
# tinycbor
include_directories(vendor/cbor/src)
set(lib_src ${lib_src}
    vendor/cbor/src/cborerrorstrings.c
    vendor/cbor/src/cborencoder.c
    vendor/cbor/src/cborencoder_close_container_checked.c
    vendor/cbor/src/cborparser.c
    vendor/cbor/src/cborparser_dup_string.c
    vendor/cbor/src/cborpretty.c
    vendor/cbor/src/cborpretty_stdio.c
    vendor/cbor/src/cborvalidation.c
)

### PLATFORM-SPECIFIC ###
if(WIN32)
    # miho-io.h windows implementation sources
    set(lib_src ${lib_src} src/miho-io-win/iocore.c)
    set(lib_src ${lib_src} src/miho-io-win/winsock.c)
    # miho-io.h windows implementation dependencies
	set(lib_link ${lib_link} mswsock)
endif()

add_executable(miho-driver ${lib_src})
target_link_libraries(miho-driver ${lib_link})
