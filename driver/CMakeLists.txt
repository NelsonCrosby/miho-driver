project(miho-driver)

set(lib_src)
set(lib_link)

### MAIN MIHO SOURCES ###
include_directories(../include)
include_directories("${miho_driver_BINARY_DIR}")
set(lib_src ${lib_src}
	external-api.c
    exceptions.c
	subsystems.c
)

### VENDORED DEPENDENCIES ###
# tinycbor
include_directories(../vendor/cbor/src)
set(lib_src ${lib_src}
    ../vendor/cbor/src/cborerrorstrings.c
    ../vendor/cbor/src/cborencoder.c
    ../vendor/cbor/src/cborencoder_close_container_checked.c
    ../vendor/cbor/src/cborparser.c
    ../vendor/cbor/src/cborparser_dup_string.c
    ../vendor/cbor/src/cborpretty.c
    ../vendor/cbor/src/cborpretty_stdio.c
    ../vendor/cbor/src/cborvalidation.c
)

### PLATFORM-SPECIFIC ###
if(WIN32)
    # miho-io.h windows implementation sources
    set(lib_src ${lib_src} platform-win/m-io-core.c)
    set(lib_src ${lib_src} platform-win/m-io-winsock.c)
    # miho-io.h windows implementation dependencies
	set(lib_link ${lib_link} mswsock)
endif()

add_library(miho-driver SHARED ${lib_src})
target_link_libraries(miho-driver ${lib_link})

generate_export_header(miho-driver
	BASE_NAME miho
	EXPORT_MACRO_NAME MIHO_EXPORT
	EXPORT_FILE_NAME miho/_export.h
	STATIC_DEFINE SHARED_EXPORTS_BUILT_AS_STATIC
)

install(
	TARGETS miho-driver
	DESTINATION "${LIBRARY_INSTALL_DIR}"
	RUNTIME DESTINATION bin
)
