project(miho-cli)

include_directories(../include)
include_directories("${miho_driver_BINARY_DIR}")

add_executable(miho-cli tempmain.c)
target_link_libraries(miho-cli miho-driver)

install(
	TARGETS miho-cli
	DESTINATION "${EXECUTABLE_INSTALL_DIR}"
	RUNTIME DESTINATION "bin"
)
