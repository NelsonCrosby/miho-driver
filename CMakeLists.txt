cmake_minimum_required(VERSION 3.0)

include(GenerateExportHeader)

project(miho)

add_subdirectory(driver)
add_subdirectory(cli)

install(
	FILES include/miho.h "${miho_driver_BINARY_DIR}/miho/_export.h"
	DESTINATION include
)
