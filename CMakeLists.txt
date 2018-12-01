cmake_minimum_required(VERSION 2.6)

project(MihoDriver)

set(lib_src src/tempmain.c)
set(lib_src src/exceptions.c)
set(lib_link)
if(WIN32)
    set(lib_src ${lib_src} src/miho-io-win/iocore.c)
    set(lib_src ${lib_src} src/miho-io-win/winsock.c)
	set(lib_link ${lib_link} mswsock)
endif()

add_executable(miho-driver ${lib_src})
target_link_libraries(miho-driver ${lib_link})
